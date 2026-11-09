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

// Teste minimo de Linphone/tool/NexusProvisioning.hpp (so QtCore; nao precisa do SDK nem dos submodulos).
// Compilar: g++ -std=c++17 -fPIC -I Linphone $(pkg-config --cflags --libs Qt6Core) tests/nexus/nexus_provisioning_test.cpp
#include "tool/NexusProvisioning.hpp"
#include <cstdio>

using namespace NexusProvisioning;
static int falhas = 0;
#define CONFERE(c)                                                                                                     \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::printf("FALHOU: %s\n", #c);                                                                           \
			++falhas;                                                                                                  \
		}                                                                                                              \
	} while (0)

int main() {
	const QString t = "q5mJ0b0zv3Yk-9d3xW_7rT1uYbqk2L8nZsA0cV4eF6g"; // gitleaks:allow (token de exemplo)
	const QString u = "https://api.givm.com.br/api/prov/" + t;
	// aceitas
	CONFERE(isGivmProvisioningUrl(u));
	CONFERE(isGivmProvisioningUrl("https://app.givm.com.br/api/prov/" + t));
	CONFERE(isGivmProvisioningUrl(" https://API.givm.com.br/api/prov/" + t + " "));
	CONFERE(isGivmProvisioningUrl("https://api.givm.com.br:443/api/prov/" + t));
	// recusadas (revisao 26/10)
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br.evil.com/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://app.givm.com.br.evil.com/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("http://api.givm.com.br/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("file:///tmp/x.xml"));
	CONFERE(!isGivmProvisioningUrl("file://api.givm.com.br/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://x@api.givm.com.br/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://evil.com@api.givm.com.br/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br:8443/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://evil.givm.com.br/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br/api/prov/" + t + "/../../x"));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br/api/prov/" + t + "?u=x"));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br/api/prov/" + t + "#x"));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br/api/prov/curto"));
	// revisao 27/10: barra final, host@evil, ponto final, IDN (a cirilico)
	CONFERE(!isGivmProvisioningUrl("https://app.givm.com.br/api/prov/" + t + "/"));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br@evil.com/api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl("https://api.givm.com.br./api/prov/" + t));
	CONFERE(!isGivmProvisioningUrl(QString::fromUtf8("https://\xD0\xB0pi.givm.com.br/api/prov/") + t));
	CONFERE(!isGivmProvisioningUrl(QString::fromUtf8("https://api.givm.com.br\xE3\x80\x82" "evil.com/api/prov/") + t));
	CONFERE(!isGivmProvisioningUrl(""));
	// nexus-config:<resto>
	CONFERE(fromNexusConfig(u) == u);
	CONFERE(fromNexusConfig("//api.givm.com.br/api/prov/" + t) == u);
	CONFERE(fromNexusConfig("api.givm.com.br/api/prov/" + t) == u);
	CONFERE(fromNexusConfig("api.givm.com.br.evil.com/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("http://api.givm.com.br/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("https://evil.com/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("file:///tmp/x.xml").isEmpty());
	CONFERE(fromNexusConfig("x@api.givm.com.br/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("api.givm.com.br:8443/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("").isEmpty());
	CONFERE(fromNexusConfig("api.givm.com.br@evil.com/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("//api.givm.com.br@evil.com/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("https://api.givm.com.br@evil.com/api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig("api.givm.com.br./api/prov/" + t).isEmpty());
	CONFERE(fromNexusConfig(QString::fromUtf8("\xD0\xB0pi.givm.com.br/api/prov/") + t).isEmpty());
	CONFERE(fromNexusConfig("api.givm.com.br/api/prov/" + t + "/").isEmpty());
	// token mascarado
	CONFERE(maskToken("nexus-config:" + u) == QString("nexus-config:https://api.givm.com.br/api/prov/q5mJ…"));
	CONFERE(!maskToken(u).contains(t));
	CONFERE(maskToken("https://api.givm.com.br/prov/" + t) == QString("https://api.givm.com.br/prov/q5mJ…"));
	CONFERE(maskToken("fetch-config=" + u + " show") == QString("fetch-config=https://api.givm.com.br/api/prov/q5mJ… show"));
	CONFERE(maskToken("sip:1000@x") == QString("sip:1000@x"));
	// base64/base64url (a URL codificada) em qualquer lugar do texto vira "<base64>"; o resto igual
	const QString b64 = QString::fromLatin1(u.toLatin1().toBase64(QByteArray::Base64UrlEncoding));
	const QString b64std = QString::fromLatin1(u.toLatin1().toBase64());
	CONFERE(maskToken(b64) == QString("<base64>"));
	CONFERE(maskToken("sip-nexus:?nexus-fetch-config=" + b64) == QString("sip-nexus:?nexus-fetch-config=<base64>"));
	CONFERE(maskToken("fetch-config=" + b64std + " show") == QString("fetch-config=<base64> show"));
	CONFERE(maskToken("fetch-config=" + QString(b64std).replace("=", "%3D")) == QString("fetch-config=<base64>"));
	CONFERE(!maskToken("x " + b64 + " y").contains(b64.left(8)));
	CONFERE(maskToken("show") == QString("show"));
	CONFERE(maskToken("sip:1000@x") == QString("sip:1000@x"));
	CONFERE(maskToken("sip:1000@api.givm.com.br;transport=tls") == QString("sip:1000@api.givm.com.br;transport=tls"));
	CONFERE(maskToken("file:///tmp/linphone/config/x.xml") ==
	        QString("file:///tmp/linphone/config/x.xml"));
	// popup/dialogo: so o host
	CONFERE(displayHost(u) == QString("api.givm.com.br"));
	CONFERE(!displayHost(u).contains(t));
	CONFERE(displayHost("nexus-x:abc") == QString("…"));
	std::printf("%s (%d falhas)\n", falhas ? "ERRO" : "OK", falhas);
	return falhas ? 1 : 0;
}
