/*
 * Copyright (c) 2026 Grupo IVM (Nexus / Voipmundo).
 * Based on linphone-desktop, Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifndef NEXUS_PROVISIONING_H_
#define NEXUS_PROVISIONING_H_

#include <QRegularExpression>
#include <QString>
#include <QUrl>

// Provisionamento remoto do givm (P5, revisao 26/10): so o givm configura conta. O provisionamento
// troca a conta SIP do app; de outro servidor seria um jeito de desviar as chamadas do usuario.
// Aceito: https://api.givm.com.br/api/prov/<token> ou https://app.givm.com.br/api/prov/<token>
// (host exato, sem http, sem usuario@, porta, query, fragmento nem barra final). Mesma regra do Nexus Android.
namespace NexusProvisioning {

inline bool isGivmProvisioningUrl(const QString &raw) {
	const QString text = raw.trimmed();
	if (text.isEmpty()) return false;
	const QUrl url(text, QUrl::StrictMode);
	if (!url.isValid() || url.scheme().toLower() != QStringLiteral("https")) return false;
	const QString host = url.host().toLower();
	if (host != QStringLiteral("api.givm.com.br") && host != QStringLiteral("app.givm.com.br")) return false;
	if (!url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443)) return false;
	if (url.hasQuery() || url.hasFragment()) return false;
	static const QRegularExpression path(QStringLiteral("^/api/prov/[A-Za-z0-9_-]{16,}$"));
	return path.match(url.path(QUrl::FullyEncoded)).hasMatch();
}

// `nexus-config:<resto>` (o que vem depois do esquema) -> URL https; vazio se nao der uma URL do givm.
inline QString fromNexusConfig(const QString &rest) {
	QString candidate = rest.trimmed();
	if (candidate.startsWith(QStringLiteral("//"))) candidate = QStringLiteral("https:") + candidate;
	else if (!candidate.contains(QStringLiteral("://")) && !candidate.contains(':'))
		candidate = QStringLiteral("https://") + candidate;
	return isGivmProvisioningUrl(candidate) ? candidate : QString();
}

// Para log: o token do link vale uma conta SIP, entao so os 4 primeiros caracteres aparecem.
// Cobre /api/prov/<token> e o caminho antigo /prov/<token> (mesma regex do Nexus Android).
// O link tambem chega em base64/base64url (sip-nexus:?nexus-fetch-config=<b64>, fetch-config=<b64>): trecho de
// 24+ caracteres de base64 (ou %XX; "=" so no fim, como padding) com maiuscula, minuscula e digito vira "<base64>".
inline QString maskToken(const QString &text) {
	static const QRegularExpression token(QStringLiteral("(/(?:api/)?prov/)([A-Za-z0-9_-]{0,4})[A-Za-z0-9_-]*"));
	static const QRegularExpression base64(QStringLiteral("[A-Za-z0-9+/_%-]{24,}(?:=|%3D){0,2}"));
	static const QRegularExpression upper(QStringLiteral("[A-Z]")), lower(QStringLiteral("[a-z]")),
	    digit(QStringLiteral("[0-9]"));
	QString masked = text;
	masked.replace(token, QStringLiteral("\\1\\2…"));
	QString out;
	qsizetype last = 0;
	for (auto it = base64.globalMatch(masked); it.hasNext();) {
		const QRegularExpressionMatch m = it.next();
		const QString run = m.captured(0);
		if (!run.contains(upper) || !run.contains(lower) || !run.contains(digit)) continue;
		out += masked.mid(last, m.capturedStart(0) - last) + QStringLiteral("<base64>");
		last = m.capturedEnd(0);
	}
	return last == 0 ? masked : out + masked.mid(last);
}

// O que aparece em popup/dialogo: so o host (nunca o caminho com o token). Sem host: "…".
inline QString displayHost(const QString &text) {
	const QString host = QUrl(text.trimmed()).host().toLower();
	return host.isEmpty() ? QStringLiteral("…") : host;
}

} // namespace NexusProvisioning

#endif // NEXUS_PROVISIONING_H_
