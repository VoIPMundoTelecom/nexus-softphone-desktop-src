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

#ifndef ONDAFONE_MODO_MODEL_H_
#define ONDAFONE_MODO_MODEL_H_

#include <QObject>
#include <QTimer>
#include <linphone++/linphone.hh>

#include "tool/OndafoneModo.hpp"
#include "tool/OndafoneModoEstado.hpp"

#include <memory>

class QNetworkAccessManager;

// Modo limitado/completo do Ondafone no Core do desktop (regras em tool/OndafoneModo.hpp), em paridade com o
// Android (core/OndafoneModoManager.kt). Só existe com ENABLE_ONDAFONE_MODOS (padrão OFF) até o build completo do
// desktop voltar a compilar no CI (espelho das dependências, #14).
//
// - Grava, por conta, a hora do último REGISTER 200 OK que qualifica (proxy da lista assinada, UDP/TCP/TLS).
//   Esse estado, a lista aceita e as horas sem registro ficam num arquivo privado do app (AppLocalDataLocation, fora do perfil móvel do Windows,
//   ondafone_modo.ini, permissão só do dono), nunca no linphonerc: provisionamento que não é do givm não forja o
//   modo completo nem muda as horas (tool/OndafoneModoEstado.hpp).
// - TLS primeiro (só com -DONDAFONE_TLS_PRIMEIRO=ON, desligado por padrão): conta num proxy da lista que também está
//   em dominiosTls passa para TLS; se o REGISTER em TLS falhar, volta ao transporte original e só tenta TLS de novo
//   depois de TLS_REPETIR_HORAS. Desligado, desfaz a troca que o app tiver feito.
// - No limitado: sem vídeo, sem presença, sem push, só G.711/Opus; guarda antes o que o usuário tinha e devolve no
//   completo.
// - Lista do givm a cada LISTA_ATUALIZAR_HORAS; o cache é verificado de novo a cada leitura.
// Tudo no thread do modelo (o do Core).
class OndafoneModoModel : public QObject {
	Q_OBJECT
public:
	OndafoneModoModel(const std::shared_ptr<linphone::Core> &core, QObject *parent = nullptr);

	OndafoneModo::Modo modo() const {
		return mModo;
	}
	bool bloqueado(OndafoneModo::Recurso recurso) const;

	void avaliar(bool aplicarSempre = false);
	void onAccountAdded(const std::shared_ptr<linphone::Account> &account);
	void onAccountRemoved(const std::shared_ptr<linphone::Account> &account);
	void onAccountRegistrationStateChanged(const std::shared_ptr<linphone::Account> &account,
	                                       linphone::RegistrationState state);
	// Um provisionamento remoto terminou (ou o app subiu sem provisionar). doGivm = a config-uri é a do givm. Lê
	// horas_sem_registro da seção [ondafone] do linphonerc só nesse caso e limpa a seção sempre.
	void provisionamentoConcluido(bool sucesso, bool doGivm);

signals:
	void modoChanged(bool limitado);

private:
	std::shared_ptr<linphone::Config> config() const;
	static std::unique_ptr<OndafoneModoEstado::Armazenamento> abrirEstado();
	OndafoneLista::Lista listaVigente() const;
	qint64 versaoAceita() const;
	bool qualifica(const std::shared_ptr<linphone::Account> &account, const OndafoneLista::Lista &lista) const;
	void prepararTransportes(const OndafoneLista::Lista &lista);
	void voltarDoTls(const std::shared_ptr<linphone::Account> &account);
	void restaurarOriginal(const std::shared_ptr<linphone::Account> &account, const QString &chave);
	void limparTls(const QString &chave);
	int antesInt(const char *k, int padrao) const;
	void gravarAntes(const char *k, int v);
	void migrarAntesDoLinphonerc();
	void limitar();
	void restaurar();
	void atualizarListaSeVencida();

	std::shared_ptr<linphone::Core> mCore;
	std::unique_ptr<OndafoneModoEstado::Armazenamento> mEstado;
	OndafoneModo::Modo mModo = OndafoneModo::Modo::Limitado;
	int mContas = 0;
	QTimer mReavaliar;
	QNetworkAccessManager *mRede = nullptr;
};

#endif // ONDAFONE_MODO_MODEL_H_
