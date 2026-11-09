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

#ifndef ONDAFONE_CONTA_CORE_H_
#define ONDAFONE_CONTA_CORE_H_

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

#include "tool/OndafoneConta.hpp"

class QNetworkAccessManager;

// Conta Ondafone no desktop (givm #390), em paridade com o Android (OndafoneEntrarActivity/OndafoneContaActivity):
// e-mail + código de 6 dígitos (cria a conta com o aceite ou entra numa existente), sair e excluir. Regras e corpos
// em tool/OndafoneConta.hpp. Vive no thread da UI (rede assíncrona do Qt) e vai para o QML como a propriedade de
// contexto "ondafoneConta". Só existe com ENABLE_ONDAFONE_CONTA (padrão OFF).
//
// Sessão: AppLocalDataLocation/ondafone/ondafone_sessao.ini, diretório 0700 e arquivo criado 0600 antes de qualquer
// token; nunca no linphonerc nem no log.
// TODO(ondafone): mover access/refresh para o cofre do sistema (QtKeychain) quando ENABLE_QT_KEYCHAIN voltar a ser
// usado no desktop (hoje está desligado e o código do keychain do upstream está comentado).
class OndafoneContaCore : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool temSessao READ temSessao NOTIFY mudou)
	Q_PROPERTY(QString email READ email NOTIFY mudou)
	Q_PROPERTY(bool ocupado READ ocupado NOTIFY mudou)
	Q_PROPERTY(bool codigoPedido READ codigoPedido NOTIFY mudou)
	Q_PROPERTY(QString emailPedido READ emailPedido NOTIFY mudou)
	Q_PROPERTY(QString aviso READ aviso NOTIFY mudou)
	Q_PROPERTY(QString privacidadeUrl READ privacidadeUrl CONSTANT)

public:
	explicit OndafoneContaCore(QObject *parent = nullptr);

	bool temSessao() const;
	QString email() const;
	bool ocupado() const {
		return mOcupado;
	}
	bool codigoPedido() const {
		return !mEmailPedido.isEmpty();
	}
	QString emailPedido() const {
		return mEmailPedido;
	}
	QString aviso() const {
		return mAviso;
	}
	QString privacidadeUrl() const {
		return OndafoneConta::PRIVACIDADE_URL;
	}

	Q_INVOKABLE bool emailValido(const QString &email) const {
		return OndafoneConta::emailValido(email);
	}
	Q_INVOKABLE bool codigoValido(const QString &codigo) const {
		return OndafoneConta::codigoValido(codigo);
	}

	// 1º passo: pede o código por e-mail (o givm responde igual exista ou não a conta).
	Q_INVOKABLE void pedirCodigo(const QString &email, bool aceite, bool marketing);
	// 2º passo: confere o código e grava a sessão.
	Q_INVOKABLE void confirmar(const QString &codigo);
	Q_INVOKABLE void voltarAoEmail();
	// Sai deste computador (todos = false) ou de todos os aparelhos; a sessão local some mesmo sem rede.
	Q_INVOKABLE void sair(bool todos = false);
	// Exclusão definitiva da conta (LGPD); a sessão local some quando o givm confirma.
	Q_INVOKABLE void excluir();

signals:
	void mudou();
	void entrou();
	void saiu();
	void excluida();

private:
	using Fim = std::function<void(int status, const QByteArray &corpo)>;

	void post(const QString &caminho, const QByteArray &corpo, const QString &bearer, const Fim &fim);
	// Requisição com Bearer: renova antes se o access estiver vencendo, e uma vez depois de um 401.
	void autenticado(const QString &caminho, const QByteArray &corpo, const Fim &fim);
	// Uma renovação por vez (o refresh é de uso único: dois pedidos com o mesmo refresh revogam a sessão).
	void renovar(const std::function<void(bool ok)> &fim);

	OndafoneConta::Sessao lerSessao() const;
	bool gravarSessao(const QByteArray &corpo);
	void gravarEmail(const QString &email);
	void apagarSessao();

	void setOcupado(bool ocupado);
	void setAviso(const QString &aviso);

	QNetworkAccessManager *mRede = nullptr;
	bool mOcupado = false;
	QString mEmailPedido;
	QString mAviso;
	bool mRenovando = false;
	QList<std::function<void(bool)>> mEsperandoRenovar;
};

#endif // ONDAFONE_CONTA_CORE_H_
