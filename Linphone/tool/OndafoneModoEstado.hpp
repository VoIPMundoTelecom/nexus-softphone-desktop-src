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

#ifndef ONDAFONE_MODO_ESTADO_H_
#define ONDAFONE_MODO_ESTADO_H_

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QXmlStreamReader>

#include "tool/OndafoneModo.hpp"

// Estado do modo limitado/completo FORA do linphonerc (revisão 05/11), em paridade com o Android
// (utils/OndafoneModoEstado.kt).
//
// O linphonerc é escrito pelo provisionamento remoto, então nada que destrava o modo completo pode morar nele: o
// último REGISTER confiável de cada conta (ok_<chave>), a lista assinada aceita (envelope e versão) e as horas sem
// registro ficam num arquivo privado do app (OndafoneModoModel: AppLocalDataLocation, permissão só do dono).
// horas_sem_registro só é aceito quando o provisionamento veio do givm (aceitarProvisionamento); a seção [ondafone]
// do linphonerc é limpa depois de cada provisionamento e a cada start.
namespace OndafoneModoEstado {

// Armazenamento privado (arquivo do app; mapa nos testes).
class Armazenamento {
public:
	virtual ~Armazenamento() = default;
	virtual QString ler(const QString &chave) const = 0;
	virtual void gravar(const QString &chave, const QString &valor) = 0; // valor vazio = apaga
	// false se alguma gravação desta sessão não chegou ao disco (sync com AccessError); memória: sempre true
	virtual bool gravacaoOk() const {
		return true;
	}
};

class Memoria : public Armazenamento {
public:
	QMap<QString, QString> mapa;
	QString ler(const QString &chave) const override {
		return mapa.value(chave);
	}
	void gravar(const QString &chave, const QString &valor) override {
		if (valor.isEmpty()) mapa.remove(chave);
		else mapa.insert(chave, valor);
	}
};

// Grava os pares e confere: true só se a escrita chegou ao disco (gravacaoOk) e a leitura devolve o gravado (no
// arquivo do app a leitura vem do cache do QSettings: quem garante o disco é gravacaoOk). Quem migra dados de outro
// lugar (linphonerc) só apaga a origem com true; com false a origem fica e a migração roda de novo no próximo start.
inline bool gravarConferindo(Armazenamento &e, const QList<QPair<QString, QString>> &pares) {
	for (const auto &[k, v] : pares)
		e.gravar(k, v);
	if (!e.gravacaoOk()) return false;
	for (const auto &[k, v] : pares)
		if (e.ler(k) != v) return false;
	return true;
}

inline const QString MODO = QStringLiteral("modo");
inline const QString LISTA_ENVELOPE = QStringLiteral("lista_envelope");
inline const QString LISTA_VERSAO_ACEITA = QStringLiteral("lista_versao_aceita");
inline const QString LISTA_BAIXADA_MS = QStringLiteral("lista_baixada_ms");
inline const QString HORAS_SEM_REGISTRO = QStringLiteral("horas_sem_registro");
inline const QString OK = QStringLiteral("ok_");
inline const QString TLS_FALHOU = QStringLiteral("tls_falhou_");
inline const QString TLS_ORIGINAL = QStringLiteral("tls_original_");
inline const QString TLS_APLICADO = QStringLiteral("tls_aplicado_");

// Seção do linphonerc que o provisionamento do givm usa para mandar horas_sem_registro (e que versões anteriores
// usavam para o estado): o app nunca confia no que está nela.
constexpr const char *SECAO_LINPHONERC = "ondafone";

inline qint64 lerNumero(const Armazenamento &e, const QString &chave) {
	bool ok = false;
	const qint64 v = e.ler(chave).toLongLong(&ok);
	return ok ? v : -1;
}

inline qint64 ultimoRegistroConfiavel(const Armazenamento &e, const QString &chaveConta) {
	return lerNumero(e, OK + chaveConta);
}

inline int horas(const Armazenamento &e) {
	bool ok = false;
	const int h = e.ler(HORAS_SEM_REGISTRO).toInt(&ok);
	return ok ? OndafoneModo::horasSemRegistro(h) : OndafoneModo::HORAS_SEM_REGISTRO_PADRAO;
}

inline qint64 versaoAceita(const Armazenamento &e) {
	const qint64 v = lerNumero(e, LISTA_VERSAO_ACEITA);
	return v < 0 ? 0 : v;
}

inline void esquecerConta(Armazenamento &e, const QString &chaveConta) {
	for (const QString &p : {OK, TLS_FALHOU, TLS_ORIGINAL, TLS_APLICADO})
		e.gravar(p + chaveConta, QString());
}

// Um provisionamento acabou de ser aplicado; secao = entradas da seção [ondafone] que ele deixou no linphonerc. Só o
// do givm (doGivm) pode mudar horas_sem_registro; nenhum provisionamento grava ok_*, lista, versão ou modo.
inline void aceitarProvisionamento(const QMap<QString, QString> &secao, bool doGivm, Armazenamento &e) {
	if (!doGivm) return;
	bool ok = false;
	const int h = secao.value(HORAS_SEM_REGISTRO).trimmed().toInt(&ok);
	e.gravar(HORAS_SEM_REGISTRO, ok ? QString::number(OndafoneModo::horasSemRegistro(h)) : QString());
}

// Entradas da seção [ondafone] de um lpconfig em XML (o formato do provisionamento). XML inválido ou com DTD = vazio.
inline QMap<QString, QString> secaoDoXml(const QString &xml) {
	QMap<QString, QString> out;
	QXmlStreamReader r(xml);
	bool naSecao = false;
	while (!r.atEnd()) {
		r.readNext();
		if (r.tokenType() == QXmlStreamReader::DTD) return {};
		if (r.isStartElement() && r.name() == QStringLiteral("section"))
			naSecao = r.attributes().value(QStringLiteral("name")) == QLatin1String(SECAO_LINPHONERC);
		else if (r.isEndElement() && r.name() == QStringLiteral("section")) naSecao = false;
		else if (naSecao && r.isStartElement() && r.name() == QStringLiteral("entry")) {
			const QString nome = r.attributes().value(QStringLiteral("name")).toString();
			out.insert(nome, r.readElementText());
		}
	}
	if (r.hasError()) return {};
	return out;
}

} // namespace OndafoneModoEstado

#endif // ONDAFONE_MODO_ESTADO_H_
