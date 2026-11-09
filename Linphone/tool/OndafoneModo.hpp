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

#ifndef ONDAFONE_MODO_H_
#define ONDAFONE_MODO_H_

#include <QCryptographicHash>
#include <QList>
#include <QSet>
#include <QString>

#include "OndafoneLista.hpp"

// Modos do Ondafone (S3, 03/11), regras puras (só QtCore), em paridade com o Android (utils/OndafoneModo.kt).
//
// - LIMITADO: qualquer provedor SIP ("Usar minha operadora"). Áudio, teclado/DTMF, mudo, viva-voz, espera,
//   histórico, contatos, chat simples, TLS/SRTP, transferência direta e os codecs G.711/Opus.
// - COMPLETO: só depois de um REGISTER 200 OK com servidor e rotas em `hosts` da lista assinada do givm
//   (OndafoneLista), em UDP, TCP ou TLS (opção B, 03/11; MODO_COMPLETO_EXIGE_TLS = false). Host também em
//   `dominiosTls` com "TLS primeiro" ligado (-DONDAFONE_TLS_PRIMEIRO=ON, desligado por padrão): o app tenta TLS primeiro e só
//   volta ao transporte original (UDP/TCP) se o TLS falhar (tentarTls). Volta ao limitado se a conta sair, mudar de host ou ficar HORAS_SEM_REGISTRO_PADRAO horas sem
//   registro confiável.
namespace OndafoneModo {

enum class Modo { Limitado, Completo };

// Recurso bloqueado no modo limitado; o slug vai no utm_campaign da oferta (ondafone_upsell_<slug>).
enum class Recurso {
	Video,
	Gravacao,
	Conferencia,
	TransferenciaAssistida,
	Presenca,
	Push,
	Provisionamento,
	ContasAdicionais,
	Codecs
};

inline QString slug(Recurso r) {
	switch (r) {
		case Recurso::Video: return QStringLiteral("video");
		case Recurso::Gravacao: return QStringLiteral("gravacao");
		case Recurso::Conferencia: return QStringLiteral("conferencia");
		case Recurso::TransferenciaAssistida: return QStringLiteral("transferencia_assistida");
		case Recurso::Presenca: return QStringLiteral("presenca");
		case Recurso::Push: return QStringLiteral("push");
		case Recurso::Provisionamento: return QStringLiteral("provisionamento");
		case Recurso::ContasAdicionais: return QStringLiteral("contas_adicionais");
		case Recurso::Codecs: return QStringLiteral("codecs");
	}
	return QString();
}

// false (padrão, opção B de 03/11): basta servidor e rotas em `hosts`, em qualquer transporte. true: exige TLS com
// certificado verificado (cadeia + nome) num domínio de `dominiosTls`.
constexpr bool MODO_COMPLETO_EXIGE_TLS = false;

// Sem registro confiável por este tempo, a conta deixa de destravar o modo completo.
constexpr int HORAS_SEM_REGISTRO_PADRAO = 24;

// Depois de o TLS falhar numa conta, o app só tenta TLS de novo nela passado este tempo.
constexpr int TLS_REPETIR_HORAS = 24;

// Intervalo entre downloads da lista (a validade de cada lista vem nela, validoAte).
constexpr int LISTA_ATUALIZAR_HORAS = 6;
// Depois de um download que falhou, tenta de novo neste tempo (não em LISTA_ATUALIZAR_HORAS).
constexpr qint64 LISTA_REPETIR_MS = 15LL * 60LL * 1000LL;

constexpr qint64 HORA_MS = 3600LL * 1000LL;

inline QString listaUrl() {
	return QStringLiteral("https://api.givm.com.br/api/ondafone/proxies");
}

// Codecs de áudio do modo limitado (mime em minúsculas); o telephone-event (DTMF RFC 4733) fica sempre.
inline bool codecPermitido(const QString &mime) {
	static const QSet<QString> permitidos = {QStringLiteral("pcmu"), QStringLiteral("pcma"), QStringLiteral("opus"),
	                                         QStringLiteral("telephone-event")};
	return permitidos.contains(mime.toLower());
}

// Um salto da sinalização: o servidor da conta ou uma rota (outbound proxy). Transporte em minúsculas ou vazio.
struct Salto {
	QString host;
	QString transporte;
	bool desviado = false; // a URI tem maddr: o pacote não vai para `host`
};

// Teto do [ondafone] horas_sem_registro vindo do lpconfig (uma semana).
constexpr int HORAS_SEM_REGISTRO_MAX = 168;

inline int horasSemRegistro(int lido) {
	return lido < 1 ? 1 : (lido > HORAS_SEM_REGISTRO_MAX ? HORAS_SEM_REGISTRO_MAX : lido);
}

inline bool emTls(const Salto &s) {
	return s.transporte.toLower() == QStringLiteral("tls");
}

// true se um REGISTER 200 OK por esta conta prova que ela está num proxy nosso: servidor e todas as rotas em
// `hosts` (o REGISTER vai pela rota); com exigeTls, todos em TLS, num domínio de `dominiosTls`, e o SDK
// verificando o certificado (cadeia) e o nome.
inline bool qualifica(const Salto &servidor,
                      const QList<Salto> &rotas,
                      bool verificaCertificado,
                      bool verificaNome,
                      const OndafoneLista::Lista &lista,
                      bool exigeTls = MODO_COMPLETO_EXIGE_TLS) {
	if (exigeTls && (!verificaCertificado || !verificaNome)) return false;
	QList<Salto> saltos = rotas;
	saltos.prepend(servidor);
	for (const Salto &s : saltos) {
		if (s.desviado || !OndafoneLista::casaAlgum(s.host, lista.hosts)) return false;
		if (exigeTls && (!emTls(s) || !OndafoneLista::casaAlgum(s.host, lista.dominiosTls))) return false;
	}
	return true;
}

// true se todos os saltos (servidor e rotas) estão em `hosts` e em `dominiosTls`: dá para usar TLS.
inline bool aceitaTls(const Salto &servidor, const QList<Salto> &rotas, const OndafoneLista::Lista &lista) {
	QList<Salto> saltos = rotas;
	saltos.prepend(servidor);
	for (const Salto &s : saltos)
		if (!OndafoneLista::casaAlgum(s.host, lista.hosts) || !OndafoneLista::casaAlgum(s.host, lista.dominiosTls))
			return false;
	return true;
}

// true se o app deve passar a conta para TLS agora: algum salto ainda não está em TLS, todos aceitam TLS e o TLS
// não falhou nesta conta nas últimas TLS_REPETIR_HORAS horas (tlsFalhouMs < 0 = nunca). Nunca o contrário: conta
// configurada em TLS não é rebaixada; só volta para UDP/TCP a conta que o próprio app passou para TLS.
inline bool tentarTls(const Salto &servidor,
                      const QList<Salto> &rotas,
                      const OndafoneLista::Lista &lista,
                      qint64 tlsFalhouMs,
                      qint64 agoraMs,
                      bool habilitado) { // "TLS primeiro" ligado no build (-DONDAFONE_TLS_PRIMEIRO=ON; padrão OFF)
	if (!habilitado) return false;
	bool jaTls = emTls(servidor);
	for (const Salto &r : rotas)
		jaTls = jaTls && emTls(r);
	if (jaTls || !aceitaTls(servidor, rotas, lista)) return false;
	return tlsFalhouMs < 0 || tlsFalhouMs > agoraMs || agoraMs - tlsFalhouMs >= TLS_REPETIR_HORAS * HORA_MS;
}

// true se um REGISTER que falhou na conta passada para TLS conta como falha do TLS (e o app volta ao transporte
// original). Sem rede não conta; 401/403 é credencial, não TLS. motivo = nome do linphone::Reason.
inline bool falhaDeTls(const QString &motivo, bool redeDisponivel) {
	const QString m = motivo.toLower();
	return redeDisponivel && m != QStringLiteral("unauthorized") && m != QStringLiteral("forbidden");
}

// Estado de uma conta do Core para decidir o modo (ultimoRegistroConfiavelMs < 0 = nunca).
struct Conta {
	bool qualifica = false;
	qint64 ultimoRegistroConfiavelMs = -1;
};

// COMPLETO se alguma conta ainda qualifica e teve um registro confiável nas últimas `horas` horas (registro no
// futuro, relógio voltando, não conta).
inline Modo modo(const QList<Conta> &contas, qint64 agoraMs, int horas = HORAS_SEM_REGISTRO_PADRAO) {
	const qint64 limiteMs = qMax(1, horas) * HORA_MS;
	for (const Conta &c : contas) {
		const qint64 u = c.ultimoRegistroConfiavelMs;
		if (c.qualifica && u >= 0 && u <= agoraMs && agoraMs - u < limiteMs) return Modo::Completo;
	}
	return Modo::Limitado;
}

inline bool bloqueado(Modo m, Recurso) {
	return m == Modo::Limitado;
}

// Provisionamento por QR/link: liberado sem conta (é como o cliente Voipmundo entra) e no completo.
inline bool provisionamentoBloqueado(Modo m, int quantidadeDeContas) {
	return m == Modo::Limitado && quantidadeDeContas > 0;
}

// Contas adicionais: no limitado, só 1 conta SIP.
inline bool novaContaBloqueada(Modo m, int quantidadeDeContas) {
	return m == Modo::Limitado && quantidadeDeContas >= 1;
}

// Link da oferta, com UTM por recurso.
inline QString urlOferta(Recurso r) {
	return QStringLiteral("https://voipmundo.com.br/?utm_source=ondafone&utm_medium=app&utm_campaign=ondafone_upsell_") +
	       slug(r);
}

// Chave estável de uma conta para guardar o último registro confiável (não guarda o endereço em claro).
inline QString chaveConta(const QString &identidade) {
	const QString id = identidade.trimmed().toLower();
	if (id.isEmpty()) return QString();
	return QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).left(12).toHex());
}

} // namespace OndafoneModo

#endif // ONDAFONE_MODO_H_
