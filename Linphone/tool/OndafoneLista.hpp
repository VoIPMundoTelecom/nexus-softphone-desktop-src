/*
 * Copyright (c) 2026 Grupo IVM (Nexus / Voipmundo).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ONDAFONE_LISTA_H_
#define ONDAFONE_LISTA_H_

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <cmath>
#include <functional>
#include <limits>

// Lista ASSINADA dos proxies "nossos" (givm, GET /api/ondafone/proxies). Mesmas regras do Ondafone Android
// (utils/OndafoneLista.kt), só QtCore, para o teste em tests/nexus.
//
// Envelope: {"payload_b64": "<base64 dos bytes do payload>", "sig_b64": "<base64 da assinatura Ed25519>"}.
// Payload: {"versao": <inteiro>, "validoAte": "<ISO-8601 UTC>", "kid": "<16 hex>", "hosts": [...],
// "dominiosTls": [...]}.
//
// Ordem: 1) Ed25519 sobre os BYTES decodificados de payload_b64 com as chaves embutidas (chavesPublicas()), antes
// de qualquer parse; 2) só então o JSON; 3) o kid do payload tem de ser o da chave que conferiu; 4) recusa se
// agora >= validoAte ou versao < última aceita (quem chama guarda a última aceita). Sem lista válida vale a
// embutida(). Itens: nome, "*.dominio" (um rótulo, como no TLS) ou "sha256:<hex>" do nome.
//
// O Ed25519 vem de fora (Verificador): no app, o do bctoolbox do SDK; no teste, o do OpenSSL. Qt não tem Ed25519.
namespace OndafoneLista {

constexpr int MAX_BYTES = 16 * 1024;
constexpr int MAX_ITENS = 200;

// (chave pública crua de 32 bytes, mensagem, assinatura de 64 bytes) -> confere?
using Verificador = std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)>;

struct Lista {
	qint64 versao = 0;
	qint64 validoAteMs = 0;
	QString kid;
	QSet<QString> hosts;
	QSet<QString> dominiosTls;
	bool operator==(const Lista &o) const {
		return versao == o.versao && validoAteMs == o.validoAteMs && kid == o.kid && hosts == o.hosts &&
		       dominiosTls == o.dominiosTls;
	}
};

struct Resultado {
	bool ok = false;
	Lista lista;
	QString motivo; // por que foi recusada (para log)
};

inline QString sha256Hex(const QByteArray &bytes) {
	return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

inline QString hashHost(const QString &host) {
	return sha256Hex(host.toUtf8());
}

// kid de uma chave pública crua (32 bytes): 16 primeiros hex do SHA-256, como no givm.
inline QString kid(const QByteArray &chavePublica) {
	return sha256Hex(chavePublica).left(16);
}

// Host em minúsculas, sem ponto final; vazio se vazio, IP ou inválido (a lista é só por nome).
inline QString normalizarHost(const QString &host) {
	QString h = host.trimmed().toLower();
	while (h.endsWith('.'))
		h.chop(1);
	static const QRegularExpression valido(
	    QStringLiteral("^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)+$"));
	if (h.isEmpty() || !valido.match(h).hasMatch()) return QString();
	static const QRegularExpression digitos(QStringLiteral("^[0-9]+$"));
	if (digitos.match(h.section('.', -1)).hasMatch()) return QString(); // IPv4
	return h;
}

inline bool itemValido(const QString &item) {
	if (item.startsWith(QStringLiteral("sha256:"))) {
		static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
		return hex.match(item.mid(7)).hasMatch();
	}
	if (item.startsWith(QStringLiteral("*."))) {
		const QString base = item.mid(2);
		return !base.isEmpty() && normalizarHost(base) == base;
	}
	return !item.isEmpty() && normalizarHost(item) == item;
}

// true se o host casa com o item: igual, curinga "*.base" (exatamente um rótulo a mais) ou SHA-256 do nome.
inline bool casa(const QString &host, const QString &item) {
	const QString h = normalizarHost(host);
	if (h.isEmpty()) return false;
	if (item.startsWith(QStringLiteral("sha256:"))) return QStringLiteral("sha256:") + hashHost(h) == item;
	if (item.startsWith(QStringLiteral("*."))) {
		const QString sufixo = item.mid(1); // ".base"
		if (!h.endsWith(sufixo)) return false;
		const QString rotulo = h.left(h.size() - sufixo.size());
		return !rotulo.isEmpty() && !rotulo.contains('.');
	}
	return h == item;
}

inline bool casaAlgum(const QString &host, const QSet<QString> &itens) {
	for (const QString &item : itens)
		if (casa(host, item)) return true;
	return false;
}

// base64 padrão ou base64url, com ou sem "="; vazio + ok=false se inválido.
inline QByteArray b64(const QString &texto, bool *ok) {
	QByteArray n = texto.trimmed().toLatin1();
	n.replace('-', '+').replace('_', '/');
	while (n.size() % 4 != 0)
		n.append('=');
	const auto r = QByteArray::fromBase64Encoding(n, QByteArray::AbortOnBase64DecodingErrors);
	*ok = r.decodingStatus == QByteArray::Base64DecodingStatus::Ok;
	return r.decoded;
}

inline bool itens(const QJsonValue &v, QSet<QString> *out) {
	if (!v.isArray()) return false;
	const QJsonArray arr = v.toArray();
	if (arr.size() > MAX_ITENS) return false;
	for (const QJsonValue &e : arr) {
		if (!e.isString()) return false;
		const QString item = e.toString().trimmed().toLower();
		if (!itemValido(item)) return false;
		out->insert(item);
	}
	return true;
}

inline Resultado recusa(const QString &motivo) {
	Resultado r;
	r.motivo = motivo;
	return r;
}

// Chaves públicas Ed25519 por kid, em base64 dos 32 bytes. [bloqueado: chave pública nova "Yw4s…" completa] o givm
// em produção já assina com o kid 0eb49c19091e1f36; a pública entra aqui (o teste confere que o kid bate). Até lá
// nenhuma lista do givm é aceita e vale a embutida().
inline QMap<QString, QString> chavesPublicasBase64() {
	return {};
}

// As de chavesPublicasBase64() com 32 bytes e kid coerente.
inline QMap<QString, QByteArray> chavesPublicas() {
	QMap<QString, QByteArray> out;
	const auto chaves = chavesPublicasBase64();
	for (auto it = chaves.cbegin(); it != chaves.cend(); ++it) {
		bool ok = false;
		const QByteArray raw = b64(it.value(), &ok);
		if (ok && raw.size() == 32 && kid(raw) == it.key()) out.insert(it.key(), raw);
	}
	return out;
}

// Cópia embutida (sem validade): os 3 proxies Voipmundo como SHA-256 do nome (o fonte público, espelho GPL, não
// cita os servidores internos; printf %s <host> | sha256sum) e domínio TLS *.voipmundo.net.
inline Lista embutida() {
	Lista l;
	l.versao = 0;
	l.validoAteMs = std::numeric_limits<qint64>::max();
	l.kid = QStringLiteral("embutida");
	l.hosts = {QStringLiteral("sha256:39c9ebc0fdf2ebf6bcfb31fed2c9f236367430294da88ed236cb56dd8f42b33d"),
	           QStringLiteral("sha256:5f6a5531e55c680542293f38b7d920243e4c0562b9edbf0cdaed442778dcc825"),
	           QStringLiteral("sha256:3eca7893ae22b8b50f0a5182709c36621727898f10de799a9dbf4df186181dce")};
	l.dominiosTls = {QStringLiteral("*.voipmundo.net")};
	return l;
}

// Verifica o envelope do givm (download novo ou cache).
inline Resultado verificar(const QByteArray &envelope,
                           qint64 agoraMs,
                           qint64 ultimaVersaoAceita,
                           const QMap<QString, QByteArray> &chaves,
                           const Verificador &ed25519) {
	if (envelope.trimmed().isEmpty()) return recusa(QStringLiteral("vazia"));
	if (envelope.size() > MAX_BYTES) return recusa(QStringLiteral("grande demais"));
	QJsonParseError erro;
	const QJsonDocument env = QJsonDocument::fromJson(envelope, &erro);
	if (erro.error != QJsonParseError::NoError || !env.isObject()) return recusa(QStringLiteral("envelope inválido"));
	const QJsonValue p64 = env.object().value(QStringLiteral("payload_b64"));
	const QJsonValue s64 = env.object().value(QStringLiteral("sig_b64"));
	if (!p64.isString() || !s64.isString()) return recusa(QStringLiteral("envelope inválido"));
	bool okP = false, okS = false;
	const QByteArray payload = b64(p64.toString(), &okP);
	const QByteArray assinatura = b64(s64.toString(), &okS);
	if (!okP || !okS) return recusa(QStringLiteral("envelope inválido"));
	if (assinatura.size() != 64) return recusa(QStringLiteral("assinatura com tamanho errado"));
	if (chaves.isEmpty() || !ed25519) return recusa(QStringLiteral("sem chave pública embutida"));

	// 1) Ed25519 sobre os BYTES decodificados, antes de qualquer parse: a chave que conferir é a do kid
	QString kidAssinante;
	for (auto it = chaves.cbegin(); it != chaves.cend(); ++it) {
		if (ed25519(it.value(), payload, assinatura)) {
			kidAssinante = it.key();
			break;
		}
	}
	if (kidAssinante.isEmpty()) return recusa(QStringLiteral("assinatura não confere"));

	// 2) só então o JSON; o kid declarado tem de ser o da chave que assinou
	const QJsonDocument doc = QJsonDocument::fromJson(payload, &erro);
	if (erro.error != QJsonParseError::NoError || !doc.isObject()) return recusa(QStringLiteral("payload inválido"));
	const QJsonObject json = doc.object();
	const QJsonValue vKid = json.value(QStringLiteral("kid"));
	if (!vKid.isString() || !chaves.contains(vKid.toString())) return recusa(QStringLiteral("kid desconhecido"));
	if (vKid.toString() != kidAssinante) return recusa(QStringLiteral("kid não confere com a chave que assinou"));

	// só inteiro de verdade (JSON number sem parte decimal, até 2^53)
	const QJsonValue vVersao = json.value(QStringLiteral("versao"));
	const double d = vVersao.toDouble(-1);
	if (!vVersao.isDouble() || d < 1 || d > 9007199254740992.0 || std::floor(d) != d)
		return recusa(QStringLiteral("versao não é inteiro positivo"));
	const qint64 versao = static_cast<qint64>(d);
	if (versao < ultimaVersaoAceita)
		return recusa(QStringLiteral("versao %1 menor que a aceita (%2)").arg(versao).arg(ultimaVersaoAceita));

	const QString textoValidade = json.value(QStringLiteral("validoAte")).toString();
	const QDateTime validoAte = QDateTime::fromString(textoValidade, Qt::ISODateWithMs);
	if (!textoValidade.endsWith('Z') || !validoAte.isValid()) return recusa(QStringLiteral("validoAte inválido"));
	const qint64 validoAteMs = validoAte.toMSecsSinceEpoch();
	if (agoraMs >= validoAteMs) return recusa(QStringLiteral("vencida"));

	Resultado r;
	if (!itens(json.value(QStringLiteral("hosts")), &r.lista.hosts)) return recusa(QStringLiteral("hosts inválido"));
	if (r.lista.hosts.isEmpty()) return recusa(QStringLiteral("sem hosts"));
	if (!itens(json.value(QStringLiteral("dominiosTls")), &r.lista.dominiosTls))
		return recusa(QStringLiteral("dominiosTls inválido"));
	r.ok = true;
	r.lista.versao = versao;
	r.lista.validoAteMs = validoAteMs;
	r.lista.kid = kidAssinante;
	return r;
}

// Lista que vale agora: o envelope (cache) se passar em verificar(), senão a embutida(). Se o cache for uma lista
// assinada que só venceu, a embutida perde os hosts que essa lista já tinha tirado (tirar um proxy da lista não volta
// atrás quando o givm fica fora do ar).
inline Lista vigente(const QByteArray &envelope,
                     qint64 agoraMs,
                     qint64 ultimaVersaoAceita,
                     const QMap<QString, QByteArray> &chaves,
                     const Verificador &ed25519) {
	const Resultado r = verificar(envelope, agoraMs, ultimaVersaoAceita, chaves, ed25519);
	if (r.ok) return r.lista;
	Lista e = embutida();
	const Resultado vencida =
	    verificar(envelope, std::numeric_limits<qint64>::min(), ultimaVersaoAceita, chaves, ed25519);
	if (!vencida.ok) return e;
	QSet<QString> ficam;
	for (const QString &h : e.hosts)
		for (const QString &x : vencida.lista.hosts)
			if (x == h || casa(x, h)) {
				ficam.insert(h);
				break;
			}
	e.hosts = ficam;
	return e;
}

} // namespace OndafoneLista

#endif // ONDAFONE_LISTA_H_
