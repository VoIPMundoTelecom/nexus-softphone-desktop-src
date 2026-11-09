import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control
import Linphone
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle

// Nexus: conta Ondafone (givm #390). E-mail + código de 6 dígitos (o mesmo fluxo cria a conta, com o aceite, ou entra
// numa existente), sair e excluir. "ondafoneConta" é a propriedade de contexto do App (OndafoneContaCore); só existe
// com ENABLE_ONDAFONE_CONTA, e sem ela este layout nem aparece no menu (SettingsPage.qml).
AbstractSettingsLayout {
	id: mainItem
	width: parent?.width
	saveButtonVisible: false
	contentModel: [
		{
			title: "",
			subTitle: "",
			contentComponent: content
		}
	]

	Dialog {
		id: confirmarExclusao
		text: "A conta Ondafone e os dados dela serão excluídos de forma definitiva. Deseja continuar?"
		onAccepted: ondafoneConta.excluir()
	}

	Component {
		id: content
		ColumnLayout {
			spacing: Utils.getSizeWithScreenRatio(20)
			property bool conectada: !!(ondafoneConta && ondafoneConta.temSessao)

			// -------------------------------------------------------------------------------- com sessão
			ColumnLayout {
				visible: parent.conectada
				spacing: Utils.getSizeWithScreenRatio(15)
				Text {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					text: ondafoneConta && ondafoneConta.email
						? "Conectado como %1".arg(ondafoneConta.email)
						: "Conectado à conta Ondafone"
					font: Typography.p1
					color: DefaultStyle.main2_600
				}
				RowLayout {
					spacing: Utils.getSizeWithScreenRatio(20)
					MediumButton {
						style: ButtonStyle.tertiary
						text: "Sair deste computador"
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado)
						onClicked: ondafoneConta.sair(false)
					}
					MediumButton {
						style: ButtonStyle.tertiary
						text: "Sair de todos os aparelhos"
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado)
						onClicked: ondafoneConta.sair(true)
					}
					MediumButton {
						style: ButtonStyle.main
						text: "Excluir conta"
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado)
						onClicked: confirmarExclusao.open()
					}
				}
			}

			// -------------------------------------------------------------------------------- aviso (com e sem sessão)
			Text {
				Layout.fillWidth: true
				wrapMode: Text.WordWrap
				visible: text.length > 0
				text: ondafoneConta ? ondafoneConta.aviso : ""
				font: Typography.p2l
				color: DefaultStyle.main2_600
			}

			// -------------------------------------------------------------------------------- sem sessão
			ColumnLayout {
				visible: !parent.conectada
				spacing: Utils.getSizeWithScreenRatio(15)
				Text {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					text: "Entre com seu e-mail. Enviaremos um código de 6 dígitos; se ainda não tiver conta, ela é criada."
					font: Typography.p1
					color: DefaultStyle.main2_600
				}
				FormItemLayout {
					label: "E-mail"
					Layout.fillWidth: true
					contentItem: TextField {
						id: emailEdit
						width: parent.width
						enabled: !!(ondafoneConta && !ondafoneConta.codigoPedido && !ondafoneConta.ocupado)
						Accessible.name: "E-mail"
					}
				}
				RowLayout {
					spacing: Utils.getSizeWithScreenRatio(10)
					enabled: !!(ondafoneConta && !ondafoneConta.codigoPedido && !ondafoneConta.ocupado)
					CheckBox {
						id: aceite
						checked: false
						Accessible.name: "Aceito a política de privacidade"
					}
					Text {
						Layout.fillWidth: true
						wrapMode: Text.WordWrap
						text: "Li e aceito a <a href=\"%1\">política de privacidade</a> (obrigatório para criar a conta)".arg(ondafoneConta ? ondafoneConta.privacidadeUrl : "")
						textFormat: Text.StyledText
						onLinkActivated: (link) => Qt.openUrlExternally(link)
						font: Typography.p2l
						color: DefaultStyle.main2_600
						MouseArea {
							anchors.fill: parent
							acceptedButtons: Qt.NoButton
							cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
						}
					}
				}
				RowLayout {
					spacing: Utils.getSizeWithScreenRatio(10)
					enabled: !!(ondafoneConta && !ondafoneConta.codigoPedido && !ondafoneConta.ocupado)
					CheckBox {
						id: marketing
						checked: false // consentimento separado, começa desmarcado
						Accessible.name: "Quero receber novidades e ofertas por e-mail"
					}
					Text {
						Layout.fillWidth: true
						wrapMode: Text.WordWrap
						text: "Quero receber novidades e ofertas por e-mail (opcional)"
						font: Typography.p2l
						color: DefaultStyle.main2_600
					}
				}
				FormItemLayout {
					visible: !!(ondafoneConta && ondafoneConta.codigoPedido)
					label: "Código"
					Layout.fillWidth: true
					contentItem: TextField {
						id: codigoEdit
						width: parent.width
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado)
						inputMethodHints: Qt.ImhDigitsOnly
						Accessible.name: "Código de 6 dígitos"
						// voltou ao passo do e-mail (trocar e-mail, aceite ou aparelho recusados): o código velho some
						onVisibleChanged: if (!visible) text = ""
					}
				}
				RowLayout {
					spacing: Utils.getSizeWithScreenRatio(20)
					MediumButton {
						style: ButtonStyle.main
						text: ondafoneConta && ondafoneConta.codigoPedido ? "Entrar" : "Receber código"
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado
							&& (ondafoneConta.codigoPedido ? ondafoneConta.codigoValido(codigoEdit.text)
							                               : ondafoneConta.emailValido(emailEdit.text)))
						onClicked: {
							if (ondafoneConta.codigoPedido) ondafoneConta.confirmar(codigoEdit.text)
							else ondafoneConta.pedirCodigo(emailEdit.text, aceite.checked, marketing.checked)
						}
					}
					MediumButton {
						visible: !!(ondafoneConta && ondafoneConta.codigoPedido)
						style: ButtonStyle.tertiary
						text: "Trocar e-mail"
						enabled: !!(ondafoneConta && !ondafoneConta.ocupado)
						onClicked: {
							codigoEdit.text = ""
							ondafoneConta.voltarAoEmail()
						}
					}
				}
			}
		}
	}
}
