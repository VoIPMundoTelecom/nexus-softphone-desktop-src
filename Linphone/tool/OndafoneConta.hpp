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

#ifndef ONDAFONE_CONTA_H_
#define ONDAFONE_CONTA_H_

// Conta do app no givm (back #390, /api/ondafone/...), em paridade com o Android (utils/OndafoneConta.kt e
// core/OndafoneSessao.kt): entrar por e-mail + código de 6 dígitos (cadastro e login são o mesmo fluxo), sessão com
// refresh em rotação, sair e excluir. Só regras puras e corpos JSON (QtCore), testados em
// tests/nexus/ondafone_conta_test.cpp sem o SDK. A rede e a tela ficam em core/ondafone (ENABLE_ONDAFONE_CONTA).

#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTimeZone>

namespace OndafoneConta {

inline const QString API = QStringLiteral("https://api.givm.com.br/api");
inline const QString HOST = QStringLiteral("api.givm.com.br");
// Versão dos termos aceita no cadastro (o givm só confere se ONDAFONE_TERMOS_VERSAO estiver definida).
inline const QString TERMOS_VERSAO = QStringLiteral("2026-11");
inline const QString ORIGEM = QStringLiteral("app-desktop");
// Política de privacidade (a mesma do Android: website_privacy_policy_url).
inline const QString PRIVACIDADE_URL = QStringLiteral("https://voipmundo.com.br/politica-de-privacidade/");

// O givm (#390) só aceita dispositivo.plataforma em ^(android|ios|web)$ e "dispositivo" é opcional. O desktop não é
// nenhum dos três: até o givm aceitar "desktop", o app NÃO manda "dispositivo" (a sessão fica sem aparelho; sem push
// no desktop, nada se perde). Quando o givm aceitar, basta trocar PLATAFORMA para "desktop" e incluí-la na lista.
inline const QString PLATAFORMA = QString();
inline const QStringList PLATAFORMAS_DO_GIVM = {QStringLiteral("android"), QStringLiteral("ios"),
                                                QStringLiteral("web")};

// Renova um pouco antes de vencer, para o access não vencer no meio da requisição.
constexpr qint64 MARGEM_MS = 60 * 1000;
constexpr int MAX_BYTES = 64 * 1024;

inline QString normalizarEmail(const QString &bruto) {
	return bruto.trimmed().toLower();
}

inline bool emailValido(const QString &bruto) {
	static const QRegularExpression re(QStringLiteral("^[^@\\s]{1,64}@[^@\\s.]+(\\.[^@\\s.]+)+$"));
	const QString e = normalizarEmail(bruto);
	return e.size() >= 6 && e.size() <= 254 && re.match(e).hasMatch();
}

inline QString normalizarCodigo(const QString &bruto) {
	QString out;
	for (const QChar c : bruto)
		if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) out += c;
	return out;
}

inline bool codigoValido(const QString &bruto) {
	return normalizarCodigo(bruto).size() == 6;
}

inline QByteArray json(const QJsonObject &o) {
	return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

// {"plataforma","versao"} ou vazio se a plataforma não for uma das que o givm aceita (ver PLATAFORMA).
inline QJsonObject dispositivo(const QString &plataforma, const QString &versaoApp) {
	if (!PLATAFORMAS_DO_GIVM.contains(plataforma)) return {};
	QJsonObject d{{QStringLiteral("plataforma"), plataforma}};
	if (!versaoApp.isEmpty()) d.insert(QStringLiteral("versao"), versaoApp.left(30));
	return d;
}

// POST /ondafone/cadastro: conta nova exige o aceite; o consentimento de marketing é separado (padrão não).
inline QByteArray corpoCadastro(const QString &email, bool aceite, bool marketing) {
	QJsonObject o{{QStringLiteral("email"), normalizarEmail(email)},
	              {QStringLiteral("origem"), ORIGEM},
	              {QStringLiteral("marketing"), marketing}};
	if (aceite) {
		o.insert(QStringLiteral("aceiteTermos"), true);
		o.insert(QStringLiteral("versaoTermos"), TERMOS_VERSAO);
	}
	return json(o);
}

// POST /ondafone/cadastro/confirmar
inline QByteArray corpoConfirmar(const QString &email,
                                 const QString &codigo,
                                 const QString &versaoApp,
                                 const QString &plataforma = PLATAFORMA) {
	QJsonObject o{{QStringLiteral("email"), normalizarEmail(email)},
	              {QStringLiteral("codigo"), normalizarCodigo(codigo)}};
	const QJsonObject d = dispositivo(plataforma, versaoApp);
	if (!d.isEmpty()) o.insert(QStringLiteral("dispositivo"), d);
	return json(o);
}

// POST /ondafone/sessao/renovar (rotação: o refresh antigo deixa de valer; reapresentado, o givm revoga a sessão).
inline QByteArray corpoRenovar(const QString &refreshToken, const QString &plataforma = PLATAFORMA) {
	QJsonObject o{{QStringLiteral("refreshToken"), refreshToken}};
	const QJsonObject d = dispositivo(plataforma, QString());
	if (!d.isEmpty()) o.insert(QStringLiteral("dispositivo"), d);
	return json(o);
}

// POST /ondafone/sessao/sair (Bearer)
inline QByteArray corpoSair(bool todos) {
	return json(QJsonObject{{QStringLiteral("todos"), todos}});
}

// Erro do givm: {"detail": {"motivo", "mensagem"}}; a mensagem já vem pronta para o usuário. status 0 = sem rede.
struct Erro {
	int status = 0;
	QString motivo;
	QString mensagem;
};

inline Erro lerErro(int status, const QByteArray &corpo) {
	Erro e;
	e.status = status;
	const QJsonDocument doc = QJsonDocument::fromJson(corpo);
	if (!doc.isObject()) return e;
	const QJsonValue detail = doc.object().value(QStringLiteral("detail"));
	if (!detail.isObject()) return e;
	const QJsonObject d = detail.toObject();
	if (d.value(QStringLiteral("motivo")).isString()) e.motivo = d.value(QStringLiteral("motivo")).toString().trimmed();
	if (d.value(QStringLiteral("mensagem")).isString())
		e.mensagem = d.value(QStringLiteral("mensagem")).toString().trimmed();
	return e;
}

// Texto para o usuário: o do givm, ou um genérico (sem rede / erro).
inline QString mensagem(const Erro &e) {
	if (!e.mensagem.isEmpty()) return e.mensagem;
	if (e.status == 0) return QStringLiteral("Sem conexão com o servidor. Tente de novo.");
	return QStringLiteral("Não foi possível concluir agora. Tente de novo.");
}

// expiraEm do givm é datetime.isoformat() do Python, com microssegundos e fuso ("...T21:00:00.123456+00:00").
inline qint64 isoParaMs(const QString &iso) {
	if (iso.trimmed().isEmpty()) return -1;
	QString s = iso.trimmed();
	// O Qt lê até milissegundos: corta a fração em 3 dígitos antes do fuso.
	static const QRegularExpression fracao(QStringLiteral("(\\.\\d{3})\\d+"));
	s.replace(fracao, QStringLiteral("\\1"));
	const QDateTime t = QDateTime::fromString(s, Qt::ISODateWithMs);
	if (!t.isValid()) return -1;
	// sem fuso = UTC (o givm sempre manda o fuso; não confia no fuso local da máquina)
	if (t.timeSpec() == Qt::LocalTime) return QDateTime(t.date(), t.time(), QTimeZone::utc()).toMSecsSinceEpoch();
	return t.toMSecsSinceEpoch();
}

struct Sessao {
	QString access;
	QString refresh;
	qint64 expiraMs = -1;
	bool valida() const {
		return !access.isEmpty() && !refresh.isEmpty() && expiraMs > 0;
	}
};

// Par devolvido por /cadastro/confirmar, /login/google e /sessao/renovar. Inválida se faltar algum campo.
inline Sessao lerSessao(const QByteArray &corpo) {
	Sessao s;
	const QJsonDocument doc = QJsonDocument::fromJson(corpo);
	if (!doc.isObject()) return s;
	const QJsonObject o = doc.object();
	s.access = o.value(QStringLiteral("accessToken")).toString();
	s.refresh = o.value(QStringLiteral("refreshToken")).toString();
	s.expiraMs = isoParaMs(o.value(QStringLiteral("expiraEm")).toString());
	if (!s.valida()) return Sessao();
	return s;
}

// E-mail da conta na resposta (usuario.email), só para exibir.
inline QString emailDaResposta(const QByteArray &corpo) {
	const QJsonObject u =
	    QJsonDocument::fromJson(corpo).object().value(QStringLiteral("usuario")).toObject();
	return normalizarEmail(u.value(QStringLiteral("email")).toString());
}

// O access ainda serve (sem renovar) se falta mais que a margem para vencer.
inline bool accessServe(const Sessao &s, qint64 agoraMs) {
	return !s.access.isEmpty() && s.expiraMs > 0 && agoraMs < s.expiraMs - MARGEM_MS;
}

// O que fazer com a sessão local depois de /sessao/renovar.
enum class AposRenovar { Gravar, Apagar, Manter };
inline AposRenovar aposRenovar(int status, bool corpoValido) {
	if (status >= 200 && status < 300) return corpoValido ? AposRenovar::Gravar : AposRenovar::Manter;
	// Refresh recusado (vencido, revogado ou reusado: o #390 revoga a sessão inteira): entra de novo.
	if (status == 401 || status == 403) return AposRenovar::Apagar;
	return AposRenovar::Manter; // sem rede, 429, 5xx: tenta depois com o mesmo refresh
}

// Sair: a sessão local some sempre; para o usuário dá certo também sem rede (0) e com sessão já morta (401).
inline bool sairDeuCerto(int status) {
	return (status >= 200 && status < 300) || status == 0 || status == 401;
}

} // namespace OndafoneConta

#endif // ONDAFONE_CONTA_H_
