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

#include "OndafoneModoModel.hpp"
#include "config.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QStringList>
#include <QUrl>
#include <bctoolbox/crypto.h>
#include <bctoolbox/logging.h>

using namespace OndafoneModo;
namespace E = OndafoneModoEstado;

namespace {
// TLS primeiro: -DONDAFONE_TLS_PRIMEIRO=ON (config.h); desligado por padrão até os proxies terem 5061 com certificado.
#ifdef ONDAFONE_TLS_PRIMEIRO
constexpr bool TLS_LIGADO = true;
#else
constexpr bool TLS_LIGADO = false;
#endif
// Versões anteriores guardavam no linphonerc o que o usuário tinha antes do limitado: migrado uma vez para o arquivo
// privado (chaves "antes_*") e a seção é limpa.
constexpr const char *SECAO_ANTES_LEGADA = "ondafone_antes_limitado";
const QString ANTES = QStringLiteral("antes_");
// chaves inteiras guardadas e o padrão de cada uma (o mesmo de restaurar())
const QList<QPair<const char *, int>> ANTES_INTS = {{"video_captura", 1},       {"video_exibicao", 1},
                                                    {"video_iniciar", 0},       {"video_aceitar", 1},
                                                    {"presenca_assinatura", 1}, {"salvo", 1}};

qint64 agoraMs() {
	return QDateTime::currentMSecsSinceEpoch();
}

// Ed25519 do bctoolbox (decaf, o mesmo do LIME no SDK).
bool ed25519(const QByteArray &pub, const QByteArray &msg, const QByteArray &sig) {
	if (pub.size() != 32 || sig.size() != 64) return false;
	bctbx_EDDSAContext_t *ctx = bctbx_CreateEDDSAContext(BCTBX_EDDSA_25519);
	if (!ctx) return false;
	bctbx_EDDSA_setPublicKey(ctx, reinterpret_cast<const uint8_t *>(pub.constData()), static_cast<size_t>(pub.size()));
	const int r = bctbx_EDDSA_verify(ctx, reinterpret_cast<const uint8_t *>(msg.constData()),
	                                 static_cast<size_t>(msg.size()), nullptr, 0,
	                                 reinterpret_cast<const uint8_t *>(sig.constData()), static_cast<size_t>(sig.size()));
	bctbx_DestroyEDDSAContext(ctx);
	return r == BCTBX_VERIFY_SUCCESS;
}

Salto salto(const std::shared_ptr<const linphone::Address> &a) {
	if (!a) return {};
	QString t;
	switch (a->getTransport()) {
		case linphone::TransportType::Udp: t = QStringLiteral("udp"); break;
		case linphone::TransportType::Tcp: t = QStringLiteral("tcp"); break;
		case linphone::TransportType::Tls: t = QStringLiteral("tls"); break;
		case linphone::TransportType::Dtls: t = QStringLiteral("dtls"); break;
	}
	// maddr manda o pacote para outro destino que não o nome da URI: não prova que o REGISTER chegou num proxy nosso
	return {QString::fromStdString(a->getDomain()), t, a->hasUriParam("maddr")};
}

QList<Salto> rotas(const std::shared_ptr<const linphone::AccountParams> &p) {
	QList<Salto> out;
	for (const auto &r : p->getRoutesAddresses())
		out << salto(r);
	return out;
}

// URIs SIP não têm espaço: um por item, separados por espaço.
std::string enderecos(const std::shared_ptr<const linphone::AccountParams> &p) {
	QStringList l;
	l << (p->getServerAddress() ? QString::fromStdString(p->getServerAddress()->asStringUriOnly()) : QString());
	for (const auto &r : p->getRoutesAddresses())
		l << QString::fromStdString(r->asStringUriOnly());
	return l.join(' ').toStdString();
}

std::shared_ptr<linphone::Address> paraTls(const std::shared_ptr<const linphone::Address> &a) {
	auto c = a->clone();
	if (c->getPort() == 5060) c->setPort(0); // porta padrão do UDP/TCP: em TLS vale a do TLS (5061/SRV)
	c->setTransport(linphone::TransportType::Tls);
	return c;
}

QString chave(const std::shared_ptr<linphone::Account> &account) {
	auto id = account->getParams()->getIdentityAddress();
	return id ? chaveConta(QString::fromStdString(id->asStringUriOnly())) : QString();
}

// Arquivo privado do app (fora do linphonerc): QSettings em INI, diretório e arquivo só do dono.
class EstadoArquivo : public E::Armazenamento {
public:
	explicit EstadoArquivo(const QString &caminho) : mS(caminho, QSettings::IniFormat), mCaminho(caminho) {
		restringir();
	}
	QString ler(const QString &chave) const override {
		return mS.value(chave).toString();
	}
	void gravar(const QString &chave, const QString &valor) override {
		if (valor.isEmpty()) mS.remove(chave);
		else mS.setValue(chave, valor);
		mS.sync();
		// só AccessError: FormatError (INI ilegível ao abrir) fica em status() para sempre, mas a escrita funciona
		if (mS.status() == QSettings::AccessError) mOk = false;
		restringir();
	}
	bool gravacaoOk() const override {
		return mOk;
	}

private:
	void restringir() {
		if (QFile::exists(mCaminho)) QFile::setPermissions(mCaminho, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
	}
	QSettings mS;
	QString mCaminho;
	bool mOk = true;
};
} // namespace

std::unique_ptr<E::Armazenamento> OndafoneModoModel::abrirEstado() {
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	QDir().mkpath(dir);
	QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
	return std::make_unique<EstadoArquivo>(dir + QStringLiteral("/ondafone_modo.ini"));
}

OndafoneModoModel::OndafoneModoModel(const std::shared_ptr<linphone::Core> &core, QObject *parent)
    : QObject(parent), mCore(core), mEstado(abrirEstado()) {
	mRede = new QNetworkAccessManager(this);
	mModo = mEstado->ler(E::MODO) == QStringLiteral("COMPLETO") ? Modo::Completo : Modo::Limitado;
	migrarAntesDoLinphonerc();
	prepararTransportes(listaVigente());
	avaliar(true);
	atualizarListaSeVencida();
	mReavaliar.setInterval(30 * 60 * 1000);
	connect(&mReavaliar, &QTimer::timeout, this, [this]() {
		avaliar();
		atualizarListaSeVencida();
	});
	mReavaliar.start();
}

std::shared_ptr<linphone::Config> OndafoneModoModel::config() const {
	return mCore->getConfig();
}

bool OndafoneModoModel::bloqueado(Recurso recurso) const {
	if (recurso == Recurso::Provisionamento) return provisionamentoBloqueado(mModo, mContas);
	if (recurso == Recurso::ContasAdicionais) return novaContaBloqueada(mModo, mContas);
	return OndafoneModo::bloqueado(mModo, recurso);
}

void OndafoneModoModel::provisionamentoConcluido(bool sucesso, bool doGivm) {
	auto cfg = config();
	if (sucesso) {
		QMap<QString, QString> secao;
		const std::string horas = cfg->getString(E::SECAO_LINPHONERC, E::HORAS_SEM_REGISTRO.toStdString(), "");
		if (!horas.empty()) secao.insert(E::HORAS_SEM_REGISTRO, QString::fromStdString(horas));
		E::aceitarProvisionamento(secao, doGivm, *mEstado);
	}
	// nada da seção fica para trás: o próximo provisionamento (de qualquer origem) começa do zero
	cfg->cleanSection(E::SECAO_LINPHONERC);
	avaliar();
}

qint64 OndafoneModoModel::versaoAceita() const {
	return E::versaoAceita(*mEstado);
}

OndafoneLista::Lista OndafoneModoModel::listaVigente() const {
	const QString envelope = mEstado->ler(E::LISTA_ENVELOPE);
	if (envelope.isEmpty()) return OndafoneLista::embutida();
	return OndafoneLista::vigente(envelope.toUtf8(), agoraMs(), versaoAceita(),
	                              OndafoneLista::chavesPublicas(), ed25519);
}

bool OndafoneModoModel::qualifica(const std::shared_ptr<linphone::Account> &account,
                                  const OndafoneLista::Lista &lista) const {
	auto p = account->getParams();
	return OndafoneModo::qualifica(salto(p->getServerAddress()), rotas(p),
	                               config()->getInt("sip", "verify_server_certs", 1) == 1,
	                               config()->getInt("sip", "verify_server_cn", 1) == 1, lista);
}

void OndafoneModoModel::onAccountAdded(const std::shared_ptr<linphone::Account> &) {
	prepararTransportes(listaVigente());
	avaliar();
}

void OndafoneModoModel::onAccountRemoved(const std::shared_ptr<linphone::Account> &account) {
	const QString c = chave(account);
	if (!c.isEmpty()) E::esquecerConta(*mEstado, c);
	avaliar();
}

void OndafoneModoModel::onAccountRegistrationStateChanged(const std::shared_ptr<linphone::Account> &account,
                                                          linphone::RegistrationState state) {
	const QString c = chave(account);
	if (state == linphone::RegistrationState::Ok && !c.isEmpty()) {
		auto servidor = account->getParams()->getServerAddress();
		if (servidor && servidor->getTransport() == linphone::TransportType::Tls)
			mEstado->gravar(E::TLS_FALHOU + c, QString());
		if (qualifica(account, listaVigente())) {
			mEstado->gravar(E::OK + c, QString::number(agoraMs()));
			bctbx_message("[Ondafone Modo] Trusted REGISTER 200 OK on a listed proxy");
		}
	} else if (state == linphone::RegistrationState::Failed) {
		voltarDoTls(account);
	}
	avaliar();
}

void OndafoneModoModel::prepararTransportes(const OndafoneLista::Lista &lista) {
	for (const auto &account : mCore->getAccountList()) {
		const QString c = chave(account);
		auto p = account->getParams();
		auto servidor = p->getServerAddress();
		if (c.isEmpty() || !servidor) continue;
		const QString aplicado = mEstado->ler(E::TLS_APLICADO + c);
		if (!aplicado.isEmpty()) {
			if (aplicado.toStdString() != enderecos(p)) limparTls(c); // o usuário mudou a conta: não mexe
			else if (!TLS_LIGADO || !aceitaTls(salto(servidor), rotas(p), lista)) restaurarOriginal(account, c);
			continue;
		}
		if (!tentarTls(salto(servidor), rotas(p), lista, E::lerNumero(*mEstado, E::TLS_FALHOU + c), agoraMs(),
		               TLS_LIGADO))
			continue;
		auto clone = p->clone();
		clone->setServerAddress(paraTls(servidor));
		std::list<std::shared_ptr<linphone::Address>> rs;
		for (const auto &r : p->getRoutesAddresses())
			rs.push_back(paraTls(r));
		clone->setRoutesAddresses(rs);
		mEstado->gravar(E::TLS_ORIGINAL + c, QString::fromStdString(enderecos(p)));
		mEstado->gravar(E::TLS_APLICADO + c, QString::fromStdString(enderecos(clone)));
		account->setParams(clone);
		bctbx_message("[Ondafone Modo] Listed proxy also in dominiosTls: trying TLS first");
	}
}

void OndafoneModoModel::voltarDoTls(const std::shared_ptr<linphone::Account> &account) {
	const QString c = chave(account);
	if (c.isEmpty()) return;
	const QString aplicado = mEstado->ler(E::TLS_APLICADO + c);
	if (aplicado.isEmpty()) return;
	if (aplicado.toStdString() != enderecos(account->getParams())) {
		limparTls(c);
		return;
	}
	QString motivo;
	switch (account->getError()) {
		case linphone::Reason::Unauthorized: motivo = QStringLiteral("Unauthorized"); break;
		case linphone::Reason::Forbidden: motivo = QStringLiteral("Forbidden"); break;
		default: motivo = QStringLiteral("Other"); break;
	}
	if (!falhaDeTls(motivo, mCore->isNetworkReachable())) return;
	mEstado->gravar(E::TLS_FALHOU + c, QString::number(agoraMs()));
	bctbx_warning("[Ondafone Modo] REGISTER over TLS failed, falling back to the original transport");
	restaurarOriginal(account, c);
}

void OndafoneModoModel::restaurarOriginal(const std::shared_ptr<linphone::Account> &account, const QString &c) {
	const QStringList linhas = mEstado->ler(E::TLS_ORIGINAL + c).split(' ');
	limparTls(c);
	if (linhas.isEmpty() || linhas.first().isEmpty()) return;
	auto factory = linphone::Factory::get();
	auto servidor = factory->createAddress(linhas.first().toStdString());
	if (!servidor) return;
	auto clone = account->getParams()->clone();
	clone->setServerAddress(servidor);
	std::list<std::shared_ptr<linphone::Address>> rs;
	for (int i = 1; i < linhas.size(); ++i)
		if (!linhas[i].isEmpty())
			if (auto a = factory->createAddress(linhas[i].toStdString())) rs.push_back(a);
	clone->setRoutesAddresses(rs);
	account->setParams(clone);
}

void OndafoneModoModel::limparTls(const QString &c) {
	mEstado->gravar(E::TLS_ORIGINAL + c, QString());
	mEstado->gravar(E::TLS_APLICADO + c, QString());
}

void OndafoneModoModel::avaliar(bool aplicarSempre) {
	const auto lista = listaVigente();
	QList<Conta> contas;
	for (const auto &account : mCore->getAccountList()) {
		const QString c = chave(account);
		contas << Conta{qualifica(account, lista), c.isEmpty() ? -1 : E::ultimoRegistroConfiavel(*mEstado, c)};
	}
	mContas = contas.size();
	const Modo novo = OndafoneModo::modo(contas, agoraMs(), E::horas(*mEstado));
	if (novo != mModo || aplicarSempre) {
		mModo = novo;
		mEstado->gravar(E::MODO, novo == Modo::Completo ? QStringLiteral("COMPLETO") : QStringLiteral("LIMITADO"));
		if (novo == Modo::Limitado) limitar();
		else restaurar();
		emit modoChanged(novo == Modo::Limitado);
	}
}

int OndafoneModoModel::antesInt(const char *k, int padrao) const {
	bool ok = false;
	const int v = mEstado->ler(ANTES + QLatin1String(k)).toInt(&ok);
	return ok ? v : padrao;
}

void OndafoneModoModel::gravarAntes(const char *k, int v) {
	mEstado->gravar(ANTES + QLatin1String(k), QString::number(v));
}

void OndafoneModoModel::migrarAntesDoLinphonerc() {
	auto cfg = config();
	// a seção legada só é limpa depois de uma gravação confirmada: se ela ainda existe, ela vale mais que o arquivo
	if (cfg->getInt(SECAO_ANTES_LEGADA, "salvo", 0) != 0) {
		// "salvo" por último: se a escrita parar no meio, o arquivo privado não fica com um "antes" pela metade
		QList<QPair<QString, QString>> pares;
		for (const auto &[k, padrao] : ANTES_INTS)
			if (QLatin1String(k) != QLatin1String("salvo"))
				pares << qMakePair(ANTES + QLatin1String(k),
				                   QString::number(cfg->getInt(SECAO_ANTES_LEGADA, k, padrao)));
		pares << qMakePair(ANTES + QStringLiteral("codecs"),
		                   QString::fromStdString(cfg->getString(SECAO_ANTES_LEGADA, "codecs", "")));
		pares << qMakePair(ANTES + QStringLiteral("salvo"), QStringLiteral("1"));
		if (!E::gravarConferindo(*mEstado, pares)) {
			// os valores migrados ficam na memória do QSettings (limitar() não grava por cima com o estado já
			// limitado, restaurar() usa os certos) e vão para o disco no próximo sync que der certo
			bctbx_warning("[Ondafone Modo] Could not write private state file, keeping legacy linphonerc section");
			return;
		}
		bctbx_message("[Ondafone Modo] User settings saved before limited mode moved out of linphonerc");
	}
	cfg->cleanSection(SECAO_ANTES_LEGADA);
}

void OndafoneModoModel::limitar() {
	if (antesInt("salvo", 0) == 0) {
		auto policy = mCore->getVideoActivationPolicy();
		gravarAntes("video_captura", mCore->videoCaptureEnabled() ? 1 : 0);
		gravarAntes("video_exibicao", mCore->videoDisplayEnabled() ? 1 : 0);
		gravarAntes("video_iniciar", policy->getAutomaticallyInitiate() ? 1 : 0);
		gravarAntes("video_aceitar", policy->getAutomaticallyAccept() ? 1 : 0);
		gravarAntes("presenca_assinatura", mCore->friendListSubscriptionEnabled() ? 1 : 0);
		QStringList codecs;
		for (const auto &pt : mCore->getAudioPayloadTypes())
			if (pt->enabled())
				codecs << QString::fromStdString(pt->getMimeType()) + '/' + QString::number(pt->getClockRate());
		mEstado->gravar(ANTES + QStringLiteral("codecs"), codecs.join(','));
		gravarAntes("salvo", 1);
	}
	mCore->enableVideoCapture(false);
	mCore->enableVideoDisplay(false);
	auto policy = mCore->getVideoActivationPolicy()->clone();
	policy->setAutomaticallyInitiate(false);
	policy->setAutomaticallyAccept(false);
	mCore->setVideoActivationPolicy(policy);
	mCore->enableFriendListSubscription(false);
	for (const auto &pt : mCore->getAudioPayloadTypes()) {
		const bool permitido = codecPermitido(QString::fromStdString(pt->getMimeType()));
		if (pt->enabled() != permitido) pt->enable(permitido);
	}
	for (const auto &account : mCore->getAccountList()) {
		auto p = account->getParams();
		if (p->getPushNotificationAllowed() || p->getRemotePushNotificationAllowed() || p->publishEnabled()) {
			auto clone = p->clone();
			clone->setPushNotificationAllowed(false);
			clone->setRemotePushNotificationAllowed(false);
			clone->enablePublish(false);
			account->setParams(clone);
		}
	}
	bctbx_message("[Ondafone Modo] Limited mode restrictions applied");
}

void OndafoneModoModel::restaurar() {
	if (antesInt("salvo", 0) == 0) return;
	mCore->enableVideoCapture(antesInt("video_captura", 1) != 0);
	mCore->enableVideoDisplay(antesInt("video_exibicao", 1) != 0);
	auto policy = mCore->getVideoActivationPolicy()->clone();
	policy->setAutomaticallyInitiate(antesInt("video_iniciar", 0) != 0);
	policy->setAutomaticallyAccept(antesInt("video_aceitar", 1) != 0);
	mCore->setVideoActivationPolicy(policy);
	mCore->enableFriendListSubscription(antesInt("presenca_assinatura", 1) != 0);
	const QString salvos = mEstado->ler(ANTES + QStringLiteral("codecs"));
	if (!salvos.isEmpty()) {
		const QStringList codecs = salvos.split(',');
		for (const auto &pt : mCore->getAudioPayloadTypes()) {
			const bool habilitado =
			    codecs.contains(QString::fromStdString(pt->getMimeType()) + '/' + QString::number(pt->getClockRate()));
			if (pt->enabled() != habilitado) pt->enable(habilitado);
		}
	}
	for (const auto &e : ANTES_INTS)
		mEstado->gravar(ANTES + QLatin1String(e.first), QString());
	mEstado->gravar(ANTES + QStringLiteral("codecs"), QString());
	bctbx_message("[Ondafone Modo] Complete mode: user settings restored");
}

void OndafoneModoModel::atualizarListaSeVencida() {
	const qint64 agora = agoraMs();
	const qint64 baixada = E::lerNumero(*mEstado, E::LISTA_BAIXADA_MS);
	if (baixada >= 0 && baixada <= agora && agora - baixada < LISTA_ATUALIZAR_HORAS * HORA_MS) return;
	mEstado->gravar(E::LISTA_BAIXADA_MS, QString::number(agora));
	QNetworkRequest req{QUrl(listaUrl())};
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	req.setRawHeader("Accept", "application/json");
	req.setTransferTimeout(15000);
	QNetworkReply *reply = mRede->get(req);
	// como no Android: não guarda mais que MAX_BYTES (corta o download em vez de bufferizar tudo)
	connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 recebido, qint64) {
		if (recebido > OndafoneLista::MAX_BYTES) reply->abort();
	});
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		// download falhou: tenta de novo em LISTA_REPETIR_MS, não em LISTA_ATUALIZAR_HORAS
		const auto repetirLogo = [this]() {
			const qint64 proxima = agoraMs() - LISTA_ATUALIZAR_HORAS * HORA_MS + LISTA_REPETIR_MS;
			mEstado->gravar(E::LISTA_BAIXADA_MS, QString::number(proxima));
		};
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QString tipo = reply->header(QNetworkRequest::ContentTypeHeader).toString().section(';', 0, 0).trimmed();
		if (reply->error() != QNetworkReply::NoError || status != 200 || tipo.toLower() != "application/json") {
			bctbx_warning("[Ondafone Modo] Signed list download failed (HTTP %d)", status);
			repetirLogo();
			return;
		}
		const QByteArray envelope = reply->read(OndafoneLista::MAX_BYTES + 1);
		const auto r = OndafoneLista::verificar(envelope, agoraMs(), versaoAceita(), OndafoneLista::chavesPublicas(),
		                                        ed25519);
		if (!r.ok) {
			bctbx_warning("[Ondafone Modo] Signed list refused (%s), keeping the last valid one",
			              r.motivo.toUtf8().constData());
			return; // lista recusada espera LISTA_ATUALIZAR_HORAS (só falha de rede repete logo)
		}
		mEstado->gravar(E::LISTA_ENVELOPE, QString::fromUtf8(envelope));
		mEstado->gravar(E::LISTA_VERSAO_ACEITA, QString::number(r.lista.versao));
		bctbx_message("[Ondafone Modo] Signed list v%lld accepted", static_cast<long long>(r.lista.versao));
		prepararTransportes(r.lista);
		avaliar();
	});
}
