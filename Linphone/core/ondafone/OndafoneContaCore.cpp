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

#include "OndafoneContaCore.hpp"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

#include "config.h"

using namespace OndafoneConta;

namespace {
constexpr const char *GRUPO = "ondafone_sessao";
constexpr const char *ACCESS = "access";
constexpr const char *REFRESH = "refresh";
constexpr const char *EXPIRA_MS = "expira_ms";
constexpr const char *EMAIL = "email";
constexpr int TIMEOUT_MS = 15000;

// Arquivo próprio da sessão (não o QSettings geral do app): AppLocalDataLocation/ondafone/ondafone_sessao.ini.
// O diretório "ondafone" é 0700 e o .ini nasce vazio e já 0600 ANTES do primeiro setValue: o refresh token nunca fica
// num arquivo com a permissão do umask (POSIX; no Windows a proteção é o perfil do usuário). O QSettings grava por QSaveFile (temporário no mesmo diretório 0700, que
// herda a permissão do arquivo existente). Os tokens nunca vão para o linphonerc nem para o log.
// Médio prazo: QtKeychain (cofre do sistema) quando ENABLE_QT_KEYCHAIN voltar a ser usado no desktop.
constexpr QFileDevice::Permissions SO_DONO = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

QString caminhoSessao() {
	// uma vez por execução (as propriedades do QML leem a sessão a cada binding)
	static const QString caminho = []() {
		const QString dir =
		    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/ondafone");
		// builds anteriores gravavam o .ini direto em AppLocalDataLocation (diretório 0755): apaga, com o lock
		const QString antigo = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
		                       QStringLiteral("/ondafone_sessao.ini");
		QFile::remove(antigo);
		QFile::remove(antigo + QStringLiteral(".lock"));
		QDir().mkpath(dir);
		bool ok = QFile::setPermissions(dir, SO_DONO | QFileDevice::ExeOwner); // 0700
		const QString arquivo = dir + QStringLiteral("/ondafone_sessao.ini");
		if (!QFile::exists(arquivo)) {
			QFile f(arquivo);
			if (f.open(QIODevice::WriteOnly)) f.close(); // vazio: nada sensível antes do chmod
		}
		ok = QFile::setPermissions(arquivo, SO_DONO) && ok; // 0600
		if (!ok) qWarning().noquote() << "[Ondafone Conta] Não deu para restringir a permissão do arquivo da sessão";
		return arquivo;
	}();
	return caminho;
}

struct ArquivoSessao : QSettings {
	ArquivoSessao() : QSettings(caminhoSessao(), QSettings::IniFormat) {
	}
	~ArquivoSessao() override {
		sync();
		QFile::setPermissions(fileName(), SO_DONO); // o QSaveFile já preserva; reforço
	}
};

QString versaoApp() {
#ifdef APPLICATION_SEMVER
	return QStringLiteral(APPLICATION_SEMVER);
#else
	return QString();
#endif
}
} // namespace

OndafoneContaCore::OndafoneContaCore(QObject *parent) : QObject(parent) {
	mRede = new QNetworkAccessManager(this);
}

// ------------------------------------------------------------------------------------------------ sessão local

bool OndafoneContaCore::temSessao() const {
	return !lerSessao().refresh.isEmpty();
}

QString OndafoneContaCore::email() const {
	ArquivoSessao s;
	s.beginGroup(GRUPO);
	return s.value(EMAIL).toString();
}

Sessao OndafoneContaCore::lerSessao() const {
	ArquivoSessao s;
	s.beginGroup(GRUPO);
	Sessao out;
	out.access = s.value(ACCESS).toString();
	out.refresh = s.value(REFRESH).toString();
	out.expiraMs = s.value(EXPIRA_MS, -1).toLongLong();
	return out;
}

bool OndafoneContaCore::gravarSessao(const QByteArray &corpo) {
	const Sessao nova = OndafoneConta::lerSessao(corpo);
	if (!nova.valida()) return false;
	ArquivoSessao s;
	s.beginGroup(GRUPO);
	s.setValue(ACCESS, nova.access);
	s.setValue(REFRESH, nova.refresh);
	s.setValue(EXPIRA_MS, nova.expiraMs);
	s.endGroup();
	s.sync();
	return true;
}

void OndafoneContaCore::gravarEmail(const QString &email) {
	ArquivoSessao s;
	s.beginGroup(GRUPO);
	if (email.isEmpty()) s.remove(EMAIL);
	else s.setValue(EMAIL, email);
}

void OndafoneContaCore::apagarSessao() {
	ArquivoSessao s;
	s.remove(GRUPO);
	s.sync();
	emit mudou();
}

void OndafoneContaCore::setOcupado(bool ocupado) {
	if (mOcupado == ocupado) return;
	mOcupado = ocupado;
	emit mudou();
}

void OndafoneContaCore::setAviso(const QString &aviso) {
	if (mAviso == aviso) return;
	mAviso = aviso;
	emit mudou();
}

// ------------------------------------------------------------------------------------------------ HTTP

void OndafoneContaCore::post(const QString &caminho, const QByteArray &corpo, const QString &bearer, const Fim &fim) {
	const QUrl url(API + caminho);
	// só https e só o host do givm (nada de redirect para outro lugar)
	if (url.scheme() != QStringLiteral("https") || url.host() != HOST || url.port() != -1) {
		qWarning().noquote() << "[Ondafone Conta] URL recusada";
		fim(0, QByteArray());
		return;
	}
	QNetworkRequest req(url);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
	req.setTransferTimeout(TIMEOUT_MS);
	req.setRawHeader("Accept", "application/json");
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
	if (!bearer.isEmpty()) req.setRawHeader("Authorization", "Bearer " + bearer.toUtf8());
	QNetworkReply *reply = mRede->post(req, corpo);
	connect(reply, &QNetworkReply::finished, this, [reply, caminho, fim]() {
		reply->deleteLater();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); // 0 = sem resposta
		QByteArray resposta = reply->read(MAX_BYTES + 1);
		if (resposta.size() > MAX_BYTES) resposta.clear();
		// nunca loga corpo nem token
		qInfo().noquote() << "[Ondafone Conta] POST" << caminho << "-> HTTP" << status;
		fim(status, resposta);
	});
}

void OndafoneContaCore::renovar(const std::function<void(bool ok)> &fim) {
	mEsperandoRenovar.append(fim);
	if (mRenovando) return;
	const Sessao atual = lerSessao();
	if (atual.refresh.isEmpty()) {
		const auto esperando = std::exchange(mEsperandoRenovar, {});
		for (const auto &f : esperando)
			f(false);
		return;
	}
	mRenovando = true;
	post(QStringLiteral("/ondafone/sessao/renovar"), corpoRenovar(atual.refresh), QString(),
	     [this](int status, const QByteArray &corpo) {
		     bool ok = false;
		     switch (aposRenovar(status, OndafoneConta::lerSessao(corpo).valida())) {
			     case AposRenovar::Gravar:
				     ok = gravarSessao(corpo);
				     break;
			     case AposRenovar::Apagar:
				     qWarning().noquote() << "[Ondafone Conta] Refresh recusado (HTTP" << status << "), sessão apagada";
				     apagarSessao();
				     break;
			     case AposRenovar::Manter:
				     qWarning().noquote() << "[Ondafone Conta] Não renovou (HTTP" << status << ")";
				     break;
		     }
		     mRenovando = false;
		     const auto esperando = std::exchange(mEsperandoRenovar, {});
		     for (const auto &f : esperando)
			     f(ok);
	     });
}

void OndafoneContaCore::autenticado(const QString &caminho, const QByteArray &corpo, const Fim &fim) {
	auto enviar = [this, caminho, corpo, fim](bool podeRepetir) {
		const QString access = lerSessao().access;
		post(caminho, corpo, access, [this, caminho, corpo, fim, podeRepetir](int status, const QByteArray &resposta) {
			if (status != 401 || !podeRepetir) {
				fim(status, resposta);
				return;
			}
			// access recusado: renova uma vez e repete
			renovar([this, caminho, corpo, fim, status, resposta](bool ok) {
				if (!ok) {
					fim(status, resposta);
					return;
				}
				post(caminho, corpo, lerSessao().access, fim);
			});
		});
	};
	const Sessao atual = lerSessao();
	if (atual.refresh.isEmpty()) {
		fim(0, QByteArray());
		return;
	}
	if (accessServe(atual, QDateTime::currentMSecsSinceEpoch())) {
		enviar(true);
		return;
	}
	renovar([enviar, fim](bool ok) {
		if (ok) enviar(true);
		else fim(0, QByteArray());
	});
}

// ------------------------------------------------------------------------------------------------ ações

void OndafoneContaCore::pedirCodigo(const QString &email, bool aceite, bool marketing) {
	if (mOcupado) return;
	if (!OndafoneConta::emailValido(email)) {
		setAviso(QStringLiteral("Informe um e-mail válido."));
		return;
	}
	const QString e = normalizarEmail(email);
	setAviso(QString());
	setOcupado(true);
	post(QStringLiteral("/ondafone/cadastro"), corpoCadastro(e, aceite, marketing), QString(),
	     [this, e](int status, const QByteArray &corpo) {
		     setOcupado(false);
		     if (status >= 200 && status < 300) {
			     mEmailPedido = e;
			     setAviso(QStringLiteral("Enviamos um código de 6 dígitos para %1.").arg(e));
			     emit mudou();
		     } else {
			     setAviso(mensagem(lerErro(status, corpo)));
		     }
	     });
}

void OndafoneContaCore::confirmar(const QString &codigo) {
	if (mOcupado || mEmailPedido.isEmpty()) return;
	if (!OndafoneConta::codigoValido(codigo)) {
		setAviso(QStringLiteral("O código tem 6 dígitos."));
		return;
	}
	const QString e = mEmailPedido;
	setAviso(QString());
	setOcupado(true);
	post(QStringLiteral("/ondafone/cadastro/confirmar"), corpoConfirmar(e, codigo, versaoApp()), QString(),
	     [this, e](int status, const QByteArray &corpo) {
		     setOcupado(false);
		     if (!(status >= 200 && status < 300)) {
			     const Erro erro = lerErro(status, corpo);
			     // conta nova sem o aceite: o givm cobra o aceite só aqui; volta ao e-mail para marcar e pedir outro
			     // givm com ONDAFONE_EXIGIR_APARELHO: conta nova sem dispositivo.aparelhoId, e o desktop ainda não tem
			     // plataforma no givm (só android|ios|web). Conta já existente entra normalmente.
			     if (erro.motivo == QStringLiteral("aparelho")) {
				     mEmailPedido.clear();
				     setAviso(QStringLiteral("Por enquanto a conta Ondafone é criada pelo app no celular. Crie a conta lá e "
				                             "depois entre aqui com o mesmo e-mail."));
				     emit mudou();
				     return;
			     }
			     if (erro.motivo == QStringLiteral("termos")) {
				     mEmailPedido.clear();
				     setAviso(QStringLiteral("Para criar a conta, aceite os termos de uso e a política de privacidade "
				                             "e peça um novo código."));
				     emit mudou();
				     return;
			     }
			     setAviso(mensagem(erro));
			     return;
		     }
		     if (!gravarSessao(corpo)) {
			     setAviso(mensagem(Erro{status, QString(), QString()}));
			     return;
		     }
		     const QString daResposta = emailDaResposta(corpo);
		     gravarEmail(daResposta.isEmpty() ? e : daResposta);
		     mEmailPedido.clear();
		     setAviso(QString());
		     emit mudou();
		     emit entrou();
	     });
}

void OndafoneContaCore::voltarAoEmail() {
	mEmailPedido.clear();
	mAviso.clear();
	emit mudou();
}

void OndafoneContaCore::sair(bool todos) {
	if (mOcupado) return;
	setAviso(QString());
	setOcupado(true);
	autenticado(QStringLiteral("/ondafone/sessao/sair"), corpoSair(todos), [this, todos](int status,
	                                                                                    const QByteArray &corpo) {
		setOcupado(false);
		if (todos && !(status >= 200 && status < 300)) {
			// "todos" só vale com o givm confirmando: sem isso os outros aparelhos continuam conectados
			if (status == 401 || !temSessao()) {
				apagarSessao();
				setAviso(QStringLiteral("Sua sessão expirou. Entre de novo para sair dos outros aparelhos."));
				emit saiu();
			} else {
				setAviso(status == 0 ? QStringLiteral("Sem conexão: os outros aparelhos continuam conectados. Tente de "
				                                      "novo.")
				                     : mensagem(lerErro(status, corpo)));
			}
			return;
		}
		apagarSessao(); // deste computador: sai também sem rede
		if (!sairDeuCerto(status)) setAviso(mensagem(lerErro(status, corpo)));
		emit saiu();
	});
}

void OndafoneContaCore::excluir() {
	if (mOcupado) return;
	setAviso(QString());
	setOcupado(true);
	autenticado(QStringLiteral("/ondafone/conta/excluir"), QByteArray("{}"),
	            [this](int status, const QByteArray &corpo) {
		            setOcupado(false);
		            if (!(status >= 200 && status < 300)) {
			            // refresh recusado no caminho: a sessão já foi apagada, precisa entrar de novo
			            setAviso(temSessao() ? mensagem(lerErro(status, corpo))
			                                 : QStringLiteral("Sua sessão expirou. Entre de novo para excluir a conta."));
			            return;
		            }
		            setAviso(QStringLiteral("Conta excluída."));
		            apagarSessao();
		            emit excluida();
	            });
}
