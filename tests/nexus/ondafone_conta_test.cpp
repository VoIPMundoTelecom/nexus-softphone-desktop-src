// Nexus: teste de tool/OndafoneConta.hpp (conta do app no givm) só com QtCore, sem o SDK.
// g++ -std=c++17 -fPIC -I Linphone tests/nexus/ondafone_conta_test.cpp $(pkg-config --cflags --libs Qt6Core)

#include "tool/OndafoneConta.hpp"

#include <QJsonArray>
#include <cstdio>

using namespace OndafoneConta;

static int falhas = 0;
#define CONFERE(cond)                                                                                                  \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::fprintf(stderr, "FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond);                                     \
			++falhas;                                                                                                  \
		}                                                                                                              \
	} while (0)

static QJsonObject obj(const QByteArray &b) {
	return QJsonDocument::fromJson(b).object();
}

static void email() {
	CONFERE(emailValido(QStringLiteral("  Maria@Exemplo.com.br ")));
	CONFERE(normalizarEmail(QStringLiteral("  Maria@Exemplo.com.br ")) == QStringLiteral("maria@exemplo.com.br"));
	CONFERE(!emailValido(QStringLiteral("maria@exemplo")));
	CONFERE(!emailValido(QStringLiteral("maria@@exemplo.com")));
	CONFERE(!emailValido(QStringLiteral("maria exemplo@x.com")));
	CONFERE(!emailValido(QStringLiteral("@exemplo.com")));
	CONFERE(!emailValido(QString()));
}

static void codigo() {
	CONFERE(codigoValido(QStringLiteral("123 456")));
	CONFERE(normalizarCodigo(QStringLiteral("123-456")) == QStringLiteral("123456"));
	CONFERE(!codigoValido(QStringLiteral("12345")));
	CONFERE(!codigoValido(QStringLiteral("1234567")));
	CONFERE(!codigoValido(QStringLiteral("abcdef")));
}

static void cadastro() {
	const QJsonObject com = obj(corpoCadastro(QStringLiteral("A@B.com"), true, false));
	CONFERE(com.value("email").toString() == QStringLiteral("a@b.com"));
	CONFERE(com.value("aceiteTermos").toBool());
	CONFERE(com.value("versaoTermos").toString() == QStringLiteral("2026-11"));
	CONFERE(com.value("marketing").isBool() && !com.value("marketing").toBool());
	CONFERE(com.value("origem").toString() == QStringLiteral("app-desktop"));
	const QJsonObject sem = obj(corpoCadastro(QStringLiteral("a@b.com"), false, true));
	CONFERE(!sem.contains("aceiteTermos"));
	CONFERE(!sem.contains("versaoTermos"));
	CONFERE(sem.value("marketing").toBool());
	CONFERE(TERMOS_VERSAO.size() <= 20 && ORIGEM.size() <= 40); // limites do givm
}

static void confirmarEDispositivo() {
	// desktop: plataforma fora do padrão do givm -> sem "dispositivo" (nunca um valor inválido)
	const QJsonObject o = obj(corpoConfirmar(QStringLiteral("A@b.com"), QStringLiteral("12 34 56"), "6.2.3"));
	CONFERE(o.value("codigo").toString() == QStringLiteral("123456"));
	CONFERE(o.value("email").toString() == QStringLiteral("a@b.com"));
	CONFERE(!o.contains("dispositivo"));
	CONFERE(!PLATAFORMAS_DO_GIVM.contains(PLATAFORMA));
	CONFERE(obj(corpoConfirmar("a@b.com", "123456", "1", QStringLiteral("desktop"))).contains("dispositivo") == false);
	// plataforma aceita: só plataforma + versão (sem dado pessoal), versão cortada em 30
	const QJsonObject w = obj(corpoConfirmar("a@b.com", "123456", QString(40, QLatin1Char('9')), QStringLiteral("web")));
	const QJsonObject d = w.value("dispositivo").toObject();
	CONFERE(d.value("plataforma").toString() == QStringLiteral("web"));
	CONFERE(d.value("versao").toString().size() == 30);
	CONFERE(d.keys().size() == 2);

	const QJsonObject r = obj(corpoRenovar(QStringLiteral("refresh-xyz")));
	CONFERE(r.value("refreshToken").toString() == QStringLiteral("refresh-xyz"));
	CONFERE(!r.contains("dispositivo"));
	CONFERE(obj(corpoSair(true)).value("todos").toBool());
	CONFERE(obj(corpoSair(false)).value("todos").isBool() && !obj(corpoSair(false)).value("todos").toBool());
}

static void erros() {
	const Erro e = lerErro(401, R"({"detail":{"motivo":"codigo","mensagem":"Código inválido ou vencido."}})");
	CONFERE(e.status == 401 && e.motivo == QStringLiteral("codigo"));
	CONFERE(mensagem(e) == QStringLiteral("Código inválido ou vencido."));
	// 422 do FastAPI (detail é lista): sem motivo, mensagem genérica
	const Erro v = lerErro(422, R"({"detail":[{"loc":["body","email"],"msg":"x"}]})");
	CONFERE(v.motivo.isEmpty() && v.mensagem.isEmpty() && !mensagem(v).isEmpty());
	CONFERE(lerErro(502, "<html>").mensagem.isEmpty());
	CONFERE(mensagem(lerErro(0, QByteArray())).contains(QStringLiteral("conexão")));
	const Erro nulo = lerErro(400, R"({"detail":{"motivo":null,"mensagem":null}})");
	CONFERE(nulo.motivo.isEmpty() && nulo.mensagem.isEmpty());
}

static void sessao() {
	// formato do datetime.isoformat() do Python, com microssegundos e fuso
	const qint64 ms = isoParaMs(QStringLiteral("2026-11-04T21:30:00.123456+00:00"));
	CONFERE(ms == QDateTime(QDate(2026, 11, 4), QTime(21, 30, 0, 123), QTimeZone::utc()).toMSecsSinceEpoch());
	CONFERE(isoParaMs(QStringLiteral("2026-11-04T18:30:00-03:00")) ==
	        QDateTime(QDate(2026, 11, 4), QTime(21, 30), QTimeZone::utc()).toMSecsSinceEpoch());
	CONFERE(isoParaMs(QStringLiteral("2026-11-04T21:30:00")) ==
	        QDateTime(QDate(2026, 11, 4), QTime(21, 30), QTimeZone::utc()).toMSecsSinceEpoch());
	CONFERE(isoParaMs(QStringLiteral("ontem")) == -1);
	CONFERE(isoParaMs(QString()) == -1);

	const Sessao s = lerSessao(R"({"accessToken":"a.b.c","refreshToken":"r.s.t","expiraEm":"2026-11-04T21:30:00+00:00",)"
	                           R"("usuario":{"email":"Maria@Exemplo.com"}})");
	CONFERE(s.valida());
	CONFERE(emailDaResposta(R"({"usuario":{"email":"Maria@Exemplo.com"}})") == QStringLiteral("maria@exemplo.com"));
	CONFERE(!lerSessao(R"({"accessToken":"a","expiraEm":"2026-11-04T21:30:00+00:00"})").valida());
	CONFERE(!lerSessao(R"({"accessToken":"a","refreshToken":"r","expiraEm":"x"})").valida());
	CONFERE(!lerSessao("nada").valida());

	CONFERE(accessServe(s, s.expiraMs - MARGEM_MS - 1));
	CONFERE(!accessServe(s, s.expiraMs - MARGEM_MS));
	CONFERE(!accessServe(Sessao(), 0));

	CONFERE(aposRenovar(200, true) == AposRenovar::Gravar);
	CONFERE(aposRenovar(200, false) == AposRenovar::Manter);
	CONFERE(aposRenovar(401, false) == AposRenovar::Apagar); // refresh reusado/revogado
	CONFERE(aposRenovar(403, false) == AposRenovar::Apagar);
	CONFERE(aposRenovar(0, false) == AposRenovar::Manter);
	CONFERE(aposRenovar(429, false) == AposRenovar::Manter);
	CONFERE(aposRenovar(503, false) == AposRenovar::Manter);

	CONFERE(sairDeuCerto(200) && sairDeuCerto(0) && sairDeuCerto(401));
	CONFERE(!sairDeuCerto(500));
}

static void constantes() {
	CONFERE(API.startsWith(QStringLiteral("https://")) && API.endsWith(QStringLiteral("/api")));
	CONFERE(PRIVACIDADE_URL.startsWith(QStringLiteral("https://")));
}

int main() {
	email();
	codigo();
	cadastro();
	confirmarEDispositivo();
	erros();
	sessao();
	constantes();
	if (falhas) {
		std::fprintf(stderr, "%d falha(s)\n", falhas);
		return 1;
	}
	std::printf("OndafoneConta: OK\n");
	return 0;
}
