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

// Teste de Linphone/tool/OndafoneLista.hpp e OndafoneModo.hpp (só QtCore + Ed25519 do OpenSSL para gerar chaves e
// assinar; não precisa do SDK nem dos submódulos). Mesmos casos do Android (OndafoneListaTest/OndafoneModoTest).
// Compilar: g++ -std=c++17 -fPIC -I Linphone tests/nexus/ondafone_modo_test.cpp
//   $(pkg-config --cflags --libs Qt6Core libcrypto)
#include "tool/OndafoneModo.hpp"
#include "tool/OndafoneModoEstado.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <openssl/evp.h>

using namespace OndafoneModo;
namespace E = OndafoneModoEstado;
using OndafoneLista::Lista;
static int falhas = 0;
#define CONFERE(c)                                                                                                     \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::printf("FALHOU (linha %d): %s\n", __LINE__, #c);                                                       \
			++falhas;                                                                                                  \
		}                                                                                                              \
	} while (0)
#define MOTIVO(r, m)                                                                                                   \
	do {                                                                                                               \
		const auto _r = (r);                                                                                           \
		if (_r.ok || _r.motivo != QStringLiteral(m)) {                                                                 \
			std::printf("FALHOU (linha %d): esperava recusa \"%s\", veio ok=%d \"%s\"\n", __LINE__, m, _r.ok,          \
			            qPrintable(_r.motivo));                                                                        \
			++falhas;                                                                                                  \
		}                                                                                                              \
	} while (0)

// --- Ed25519 do OpenSSL (o app usa o do bctoolbox)

struct Chave {
	EVP_PKEY *pkey = nullptr;
	QByteArray pub;
};

static Chave novaChave() {
	Chave c;
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
	EVP_PKEY_keygen_init(ctx);
	EVP_PKEY_keygen(ctx, &c.pkey);
	EVP_PKEY_CTX_free(ctx);
	size_t n = 32;
	c.pub.resize(32);
	EVP_PKEY_get_raw_public_key(c.pkey, reinterpret_cast<unsigned char *>(c.pub.data()), &n);
	return c;
}

static QByteArray assinar(const Chave &c, const QByteArray &msg) {
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, c.pkey);
	QByteArray sig(64, 0);
	size_t n = 64;
	EVP_DigestSign(ctx, reinterpret_cast<unsigned char *>(sig.data()), &n,
	               reinterpret_cast<const unsigned char *>(msg.constData()), msg.size());
	EVP_MD_CTX_free(ctx);
	return sig;
}

static bool verificaOpenSsl(const QByteArray &pub, const QByteArray &msg, const QByteArray &sig) {
	EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
	                                          reinterpret_cast<const unsigned char *>(pub.constData()), pub.size());
	if (!k) return false;
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	const bool ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, k) == 1 &&
	                EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(sig.constData()), sig.size(),
	                                 reinterpret_cast<const unsigned char *>(msg.constData()), msg.size()) == 1;
	EVP_MD_CTX_free(ctx);
	EVP_PKEY_free(k);
	return ok;
}

// --- envelope do givm

static const qint64 agora = 1800000000000LL; // 2027-01-15T08:00:00Z
static const Chave chave = novaChave();
static const QString kidTeste = OndafoneLista::kid(chave.pub);
static const QMap<QString, QByteArray> chaves = {{kidTeste, chave.pub}};

static QByteArray payload(const QJsonValue &versao = 7,
                          const QString &validoAte = "2027-01-16T08:00:00Z",
                          const QString &k = kidTeste,
                          const QJsonArray &hosts = {"proxy-a.voipmundo.net", "proxy-b.voipmundo.net"},
                          const QJsonArray &dominiosTls = {"*.voipmundo.net"}) {
	QJsonObject o{{"versao", versao}, {"emitidoEm", "2027-01-15T07:00:00Z"}, {"validoAte", validoAte},
	              {"kid", k},         {"hosts", hosts},                     {"dominiosTls", dominiosTls}};
	return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

static QByteArray envelope(const QByteArray &p, const QByteArray &sig, bool url = false) {
	const QByteArray::Base64Options opt = url ? QByteArray::Base64Options(QByteArray::Base64UrlEncoding |
	                                                                        QByteArray::OmitTrailingEquals)
	                                          : QByteArray::Base64Options(QByteArray::Base64Encoding);
	QJsonObject o{{"payload_b64", QString::fromLatin1(p.toBase64(opt))},
	              {"sig_b64", QString::fromLatin1(sig.toBase64(opt))}};
	return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

static QByteArray envelope(const QByteArray &p) {
	return envelope(p, assinar(chave, p));
}

static OndafoneLista::Resultado verificar(const QByteArray &env, qint64 ultima = 0, qint64 quando = agora) {
	return OndafoneLista::verificar(env, quando, ultima, chaves, verificaOpenSsl);
}

static void testesLista() {
	// lista válida
	auto r = verificar(envelope(payload()));
	CONFERE(r.ok);
	CONFERE(r.lista.versao == 7);
	CONFERE(r.lista.kid == kidTeste);
	CONFERE(r.lista.hosts == QSet<QString>({"proxy-a.voipmundo.net", "proxy-b.voipmundo.net"}));
	CONFERE(r.lista.dominiosTls == QSet<QString>({"*.voipmundo.net"}));
	CONFERE(verificar(envelope(payload(), assinar(chave, payload()), true)).ok); // base64url sem "="

	// payload adulterado ou assinado por outra chave
	const QByteArray p = payload();
	MOTIVO(verificar(envelope(payload(7, "2027-01-16T08:00:00Z", kidTeste, {"proxy-a.voipmundo.net", "sip.atacante.com"}),
	                          assinar(chave, p))),
	       "assinatura não confere");
	const Chave outra = novaChave();
	MOTIVO(verificar(envelope(p, assinar(outra, p))), "assinatura não confere");

	// assinatura conferida nos BYTES, antes do parse
	const QByteArray lixo = "nao e json";
	MOTIVO(verificar(envelope(lixo)), "payload inválido");
	const QByteArray reserializado = QJsonDocument::fromJson(p).toJson(QJsonDocument::Indented);
	MOTIVO(verificar(envelope(reserializado, assinar(chave, p))), "assinatura não confere");

	// kid
	MOTIVO(verificar(envelope(payload(7, "2027-01-16T08:00:00Z", "0123456789abcdef"))), "kid desconhecido");
	const QString kidOutra = OndafoneLista::kid(outra.pub);
	const QByteArray daOutra = payload(7, "2027-01-16T08:00:00Z", kidOutra);
	MOTIVO(verificar(envelope(daOutra, assinar(outra, daOutra))), "assinatura não confere");
	const QMap<QString, QByteArray> duas = {{kidTeste, chave.pub}, {kidOutra, outra.pub}};
	MOTIVO(OndafoneLista::verificar(envelope(daOutra), agora, 0, duas, verificaOpenSsl),
	       "kid não confere com a chave que assinou");
	MOTIVO(OndafoneLista::verificar(envelope(p), agora, 0, {}, verificaOpenSsl), "sem chave pública embutida");

	// validade
	MOTIVO(verificar(envelope(payload(7, "2027-01-15T08:00:00Z"))), "vencida");
	MOTIVO(verificar(envelope(p), 0, agora + 25 * HORA_MS), "vencida");
	MOTIVO(verificar(envelope(payload(7, "amanhã"))), "validoAte inválido");
	MOTIVO(verificar(envelope(payload(7, "2027-01-16T08:00:00"))), "validoAte inválido"); // sem fuso

	// versão monotônica
	CONFERE(verificar(envelope(payload(7)), 7).ok);
	CONFERE(verificar(envelope(payload(8)), 7).ok);
	MOTIVO(verificar(envelope(payload(6)), 7), "versao 6 menor que a aceita (7)");
	MOTIVO(verificar(envelope(payload("sha256:abc"))), "versao não é inteiro positivo");
	MOTIVO(verificar(envelope(payload(7.5))), "versao não é inteiro positivo");
	MOTIVO(verificar(envelope(payload(0))), "versao não é inteiro positivo");

	// itens inválidos rejeitam a lista inteira
	for (const QJsonArray &hosts : {QJsonArray{"proxy-a.voipmundo.net", "1.2.3.4"}, QJsonArray{"proxy-a.voipmundo.net", 5},
	                                QJsonArray{"*.*.voipmundo.net"}, QJsonArray{"proxy a.voipmundo.net"}, QJsonArray{}})
		CONFERE(!verificar(envelope(payload(7, "2027-01-16T08:00:00Z", kidTeste, hosts))).ok);
	CONFERE(!verificar(envelope(payload(7, "2027-01-16T08:00:00Z", kidTeste, {"proxy-a.voipmundo.net"}, {"*."}))).ok);

	// envelope malformado
	MOTIVO(verificar(""), "vazia");
	MOTIVO(verificar("<html>erro</html>"), "envelope inválido");
	MOTIVO(verificar("{\"payload_b64\":\"e30\"}"), "envelope inválido");
	MOTIVO(verificar(envelope(p, QByteArray(10, 'x'))), "assinatura com tamanho errado");
	MOTIVO(verificar(QByteArray(OndafoneLista::MAX_BYTES + 1, 'x')), "grande demais");

	// curinga de um rótulo e SHA-256
	CONFERE(OndafoneLista::casa("proxy-a.voipmundo.net", "*.voipmundo.net"));
	CONFERE(OndafoneLista::casa("PROXY-A.VoipMundo.net.", "*.voipmundo.net"));
	CONFERE(!OndafoneLista::casa("voipmundo.net", "*.voipmundo.net"));
	CONFERE(!OndafoneLista::casa("a.b.voipmundo.net", "*.voipmundo.net"));
	CONFERE(!OndafoneLista::casa("proxy-a.voipmundo.net.evil.com", "*.voipmundo.net"));
	CONFERE(!OndafoneLista::casa("evilvoipmundo.net", "*.voipmundo.net"));
	const QString item = "sha256:" + OndafoneLista::hashHost("proxy-c.voipmundo.net");
	CONFERE(OndafoneLista::casa("Proxy-C.VoipMundo.net.", item));
	CONFERE(!OndafoneLista::casa("proxy-c.voipmundo.net.evil.com", item));
	CONFERE(OndafoneLista::hashHost("") == QString("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

	// lista embutida e chaves embutidas (paridade com o Android)
	const Lista e = OndafoneLista::embutida();
	CONFERE(e.hosts.size() == 3);
	CONFERE(e.hosts.contains("sha256:39c9ebc0fdf2ebf6bcfb31fed2c9f236367430294da88ed236cb56dd8f42b33d"));
	CONFERE(e.dominiosTls == QSet<QString>({"*.voipmundo.net"}));
	CONFERE(!OndafoneLista::casaAlgum("sip.terceiro.com", e.hosts));
	CONFERE(OndafoneLista::chavesPublicas().size() == OndafoneLista::chavesPublicasBase64().size());
	CONFERE(kidTeste.size() == 16);

	// lista recusada deixa o app no básico
	const Salto hostDaLista{"proxy-a.voipmundo.net", "udp"};
	CONFERE(qualifica(hostDaLista, {}, false, false, OndafoneLista::vigente(envelope(p), agora, 0, chaves, verificaOpenSsl)));
	const QList<QPair<QByteArray, qint64>> recusadas = {
	    {envelope(p, assinar(outra, p)), 0},                             // assinatura inválida
	    {envelope(payload(7, "2027-01-16T08:00:00Z", "0123456789abcdef")), 0}, // kid desconhecido
	    {envelope(payload(7, "2027-01-15T08:00:00Z")), 0},              // vencida
	    {envelope(payload(6)), 7}};                                      // versão menor
	for (const auto &rc : recusadas) {
		const Lista l = OndafoneLista::vigente(rc.first, agora, rc.second, chaves, verificaOpenSsl);
		CONFERE(l.kid == "embutida");
		CONFERE(OndafoneLista::embutida().hosts.contains(l.hosts));
		const bool q = qualifica(hostDaLista, {}, false, false, l);
		CONFERE(!q);
		CONFERE(modo({Conta{q, agora - 60000}}, agora) == Modo::Limitado);
	}
}

static void testesRevisao() {
	// lista vencida não devolve host que ela tinha tirado; cache que não é lista assinada = embutida inteira
	const Lista l = OndafoneLista::vigente(envelope(payload(7, "2027-01-15T07:00:00Z")), agora, 0, chaves,
	                                       verificaOpenSsl);
	CONFERE(l.kid == "embutida");
	CONFERE(OndafoneLista::embutida().hosts.contains(l.hosts));
	CONFERE(l.hosts.isEmpty()); // a lista de teste não traz nenhum dos embutidos
	// a lista vencida ainda tem 1 dos embutidos (pelo hash): só ele fica
	const QString um = *OndafoneLista::embutida().hosts.cbegin();
	const Lista comUm = OndafoneLista::vigente(
	    envelope(payload(7, "2027-01-15T07:00:00Z", kidTeste, {um, "proxy-a.voipmundo.net"})), agora, 0, chaves,
	    verificaOpenSsl);
	CONFERE(comUm.hosts == QSet<QString>({um}));
	CONFERE(OndafoneLista::vigente("{}", agora, 0, chaves, verificaOpenSsl) == OndafoneLista::embutida());
	// teto do horas_sem_registro
	CONFERE(horasSemRegistro(24) == 24);
	CONFERE(horasSemRegistro(1000000) == HORAS_SEM_REGISTRO_MAX);
	CONFERE(horasSemRegistro(0) == 1);
}

static void testesModo() {
	Lista lista;
	lista.versao = 1;
	lista.validoAteMs = std::numeric_limits<qint64>::max();
	lista.kid = "teste";
	lista.hosts = {"proxy-a.voipmundo.net", "proxy-b.voipmundo.net", "sha256:" + OndafoneLista::hashHost("proxy-c.voipmundo.net")};
	lista.dominiosTls = {"*.voipmundo.net"};
	const Salto proxyATls{"proxy-a.voipmundo.net", "tls"};
	const qint64 hora = HORA_MS;

	// padrão opção B: host da lista sem TLS = completo
	CONFERE(!MODO_COMPLETO_EXIGE_TLS);
	for (const QString &t : {QString("udp"), QString("tcp"), QString()}) {
		const bool q = qualifica({"proxy-a.voipmundo.net", t}, {}, false, false, lista);
		CONFERE(q);
		CONFERE(modo({Conta{q, agora - hora}}, agora) == Modo::Completo);
	}
	CONFERE(qualifica({"proxy-c.voipmundo.net", "tcp"}, {}, false, false, lista)); // item por SHA-256

	// fora da lista = básico (provedor genérico, outro host do domínio, sufixo de outro domínio, IP, rota de terceiro)
	for (const Salto &s : {Salto{"sip.provedor-teste.com", "udp"}, Salto{"proxy-z.voipmundo.net", "tls"},
	                       Salto{"proxy-a.voipmundo.net.evil.com", "udp"}, Salto{"203.0.113.10", "udp"}}) {
		const bool q = qualifica(s, {}, true, true, lista);
		CONFERE(!q);
		CONFERE(modo({Conta{q, agora - hora}}, agora) == Modo::Limitado);
	}
	CONFERE(!qualifica({"proxy-a.voipmundo.net", "udp"}, {Salto{"sip.terceiro.com", "udp"}}, true, true, lista));

	// com TLS exigido (constante = true)
	CONFERE(qualifica(proxyATls, {}, true, true, lista, true));
	// nome nosso com maddr apontando para outro servidor não qualifica
	CONFERE(!qualifica(Salto{"proxy-a.voipmundo.net", "tls", true}, {}, true, true, lista, false));
	CONFERE(!qualifica(proxyATls, {Salto{"proxy-b.voipmundo.net", "tls", true}}, true, true, lista, false));
	CONFERE(!qualifica({"proxy-a.voipmundo.net", "udp"}, {}, true, true, lista, true));
	CONFERE(!qualifica(proxyATls, {}, false, true, lista, true));
	CONFERE(!qualifica(proxyATls, {}, true, false, lista, true));

	// TLS primeiro
	CONFERE(tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, lista, -1, agora, true));
	CONFERE(tentarTls({"proxy-b.voipmundo.net", "tcp"}, {}, lista, -1, agora, true));
	CONFERE(tentarTls(proxyATls, {Salto{"proxy-b.voipmundo.net", "udp"}}, lista, -1, agora, true));
	Lista semTls = lista;
	semTls.dominiosTls = {"*.voipmundo.com.br"};
	CONFERE(!tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, semTls, -1, agora, true));
	CONFERE(!tentarTls({"sip.provedor-teste.com", "udp"}, {}, lista, -1, agora, true));
	CONFERE(!tentarTls({"proxy-a.voipmundo.net", "udp"}, {Salto{"sip.terceiro.com", "udp"}}, lista, -1, agora, true));
	// nunca o contrário
	CONFERE(!tentarTls(proxyATls, {}, lista, -1, agora, true));
	CONFERE(!tentarTls({"sip.provedor-teste.com", "tls"}, {}, lista, -1, agora, true));
	// nova tentativa só depois do intervalo
	const qint64 h = TLS_REPETIR_HORAS * hora;
	CONFERE(!tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, lista, agora - hora, agora, true));
	CONFERE(!tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, lista, agora - h + 1, agora, true));
	CONFERE(tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, lista, agora - h, agora, true));
	CONFERE(tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, lista, agora + hora, agora, true));
	// só falha de TLS com rede faz voltar
	CONFERE(falhaDeTls("IOError", true));
	CONFERE(falhaDeTls("Unknown", true));
	CONFERE(falhaDeTls("", true));
	CONFERE(!falhaDeTls("IOError", false));
	CONFERE(!falhaDeTls("Unauthorized", true));
	CONFERE(!falhaDeTls("Forbidden", true));

	// modo a partir das contas
	CONFERE(modo({}, agora) == Modo::Limitado);
	CONFERE(modo({Conta{false, agora}}, agora) == Modo::Limitado);
	CONFERE(modo({Conta{true, -1}}, agora) == Modo::Limitado);
	CONFERE(modo({Conta{true, agora - 23 * hora}}, agora) == Modo::Completo);
	CONFERE(modo({Conta{true, agora - 24 * hora}}, agora) == Modo::Limitado);
	CONFERE(modo({Conta{true, agora - 2 * hora}}, agora, 1) == Modo::Limitado);
	CONFERE(modo({Conta{true, agora + hora}}, agora) == Modo::Limitado); // relógio voltou

	// regras
	CONFERE(codecPermitido("PCMU") && codecPermitido("pcma") && codecPermitido("opus") && codecPermitido("telephone-event"));
	CONFERE(!codecPermitido("G729") && !codecPermitido("speex") && !codecPermitido(""));
	CONFERE(bloqueado(Modo::Limitado, Recurso::Video) && !bloqueado(Modo::Completo, Recurso::Video));
	CONFERE(!provisionamentoBloqueado(Modo::Limitado, 0) && provisionamentoBloqueado(Modo::Limitado, 1));
	CONFERE(!provisionamentoBloqueado(Modo::Completo, 1));
	CONFERE(!novaContaBloqueada(Modo::Limitado, 0) && novaContaBloqueada(Modo::Limitado, 1));
	CONFERE(!novaContaBloqueada(Modo::Completo, 3));
	CONFERE(urlOferta(Recurso::TransferenciaAssistida) ==
	        QString("https://voipmundo.com.br/?utm_source=ondafone&utm_medium=app&utm_campaign=ondafone_upsell_transferencia_assistida"));
	CONFERE(chaveConta(" SIP:Ana@Exemplo.com ") == chaveConta("sip:ana@exemplo.com"));
	CONFERE(chaveConta("sip:ana@exemplo.com").size() == 24);
	CONFERE(!chaveConta("sip:ana@exemplo.com").contains("ana"));
	CONFERE(chaveConta("  ").isEmpty());
}

// Revisão 05/11: estado do modo fora do linphonerc; provisionamento de outro provedor não forja o completo.
static void testesEstado() {
	const qint64 agora = 1800000000000LL;
	const QString chave = chaveConta("sip:1000@proxy-a.voipmundo.net");
	const QString xmlForjado =
	    QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	                   "<config xmlns=\"http://www.linphone.org/xsds/lpconfig.xsd\"><section name=\"ondafone\">"
	                   "<entry name=\"ok_%1\">%2</entry><entry name=\"modo\">COMPLETO</entry>"
	                   "<entry name=\"lista_versao_aceita\">999999</entry>"
	                   "<entry name=\"horas_sem_registro\">168</entry></section></config>")
	        .arg(chave)
	        .arg(agora - 60000);
	const auto secao = E::secaoDoXml(xmlForjado);
	CONFERE(secao.value("ok_" + chave) == QString::number(agora - 60000)); // o XML traz mesmo o ok_ forjado
	auto modoCom = [&](const E::Armazenamento &e) {
		return modo({Conta{true, E::ultimoRegistroConfiavel(e, chave)}}, agora, E::horas(e));
	};

	E::Memoria outro;
	E::aceitarProvisionamento(secao, false, outro);
	CONFERE(outro.mapa.isEmpty());
	CONFERE(modoCom(outro) == Modo::Limitado); // XML com ok_* forjado NÃO dá modo completo
	CONFERE(E::horas(outro) == HORAS_SEM_REGISTRO_PADRAO);
	CONFERE(E::versaoAceita(outro) == 0);

	E::Memoria givm;
	E::aceitarProvisionamento(secao, true, givm);
	CONFERE(givm.mapa.keys() == QStringList{E::HORAS_SEM_REGISTRO}); // nem o givm grava ok_/lista/modo
	CONFERE(E::horas(givm) == 168);
	CONFERE(modoCom(givm) == Modo::Limitado);
	E::aceitarProvisionamento({{"horas_sem_registro", "48"}}, false, givm); // outro provedor não muda as horas
	CONFERE(E::horas(givm) == 168);
	E::aceitarProvisionamento({{"horas_sem_registro", "100000"}}, true, givm);
	CONFERE(E::horas(givm) == HORAS_SEM_REGISTRO_MAX);
	E::aceitarProvisionamento({}, true, givm);
	CONFERE(E::horas(givm) == HORAS_SEM_REGISTRO_PADRAO);

	E::Memoria visto;
	visto.gravar(E::OK + chave, QString::number(agora - 60000)); // REGISTER 200 OK visto pelo próprio app
	CONFERE(modoCom(visto) == Modo::Completo);
	E::esquecerConta(visto, chave);
	CONFERE(modoCom(visto) == Modo::Limitado);

	CONFERE(E::secaoDoXml("<config><section").isEmpty());
	CONFERE(E::secaoDoXml("<?xml version=\"1.0\"?><!DOCTYPE c [<!ENTITY h \"99\">]><config><section "
	                      "name=\"ondafone\"><entry name=\"horas_sem_registro\">&h;</entry></section></config>")
	            .isEmpty());

	// TLS primeiro desligado: não troca o transporte
	Lista l;
	l.hosts = {"proxy-a.voipmundo.net"};
	l.dominiosTls = {"*.voipmundo.net"};
	CONFERE(!tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, l, -1, agora, false));
	CONFERE(tentarTls({"proxy-a.voipmundo.net", "udp"}, {}, l, -1, agora, true));
}

// Armazenamento que perde as gravações (disco cheio, arquivo sem permissão).
class EstadoQuebrado : public E::Memoria {
public:
	bool falhar = true;
	void gravar(const QString &chave, const QString &valor) override {
		if (!falhar) E::Memoria::gravar(chave, valor);
	}
	bool gravacaoOk() const override {
		return !falhar;
	}
};

static void testesGravarConferindo() {
	const QList<QPair<QString, QString>> pares = {{"antes_video_captura", "1"}, {"antes_codecs", ""},
	                                              {"antes_salvo", "1"}};
	E::Memoria ok;
	CONFERE(E::gravarConferindo(ok, pares));
	CONFERE(ok.ler("antes_salvo") == "1");
	CONFERE(!ok.mapa.contains("antes_codecs")); // vazio = apaga, e conta como gravado

	EstadoQuebrado quebrado;
	CONFERE(!E::gravarConferindo(quebrado, pares)); // a origem (linphonerc) não pode ser limpa
	quebrado.falhar = false;
	CONFERE(E::gravarConferindo(quebrado, pares)); // no próximo start, com o disco ok, migra
}

int main() {
	testesGravarConferindo();
	testesEstado();
	testesLista();
	testesModo();
	testesRevisao();
	std::printf("%s (%d falhas)\n", falhas ? "ERRO" : "OK", falhas);
	return falhas ? 1 : 0;
}
