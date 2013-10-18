#include "receiverequestdialog.h"
#include "ui_receiverequestdialog.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QPixmap>
#include <QClipboard>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif

#include "bitcoin-config.h" /* for USE_QRCODE */

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

QRImageWidget::QRImageWidget(QWidget *parent):
    QLabel(parent)
{
    setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
    addAction(saveImageAction);
    QAction *copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
    addAction(copyImageAction);
}

QImage QRImageWidget::exportImage()
{
    return pixmap()->toImage().scaled(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE);
}

void QRImageWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        QMimeData *mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    }
}

void QRImageWidget::saveImage()
{
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Images (*.png)"));
    if (!fn.isEmpty())
    {
        exportImage().save(fn);
    }
}

void QRImageWidget::copyImage()
{
    QApplication::clipboard()->setImage(exportImage());
}

ReceiveRequestDialog::ReceiveRequestDialog(const SendCoinsRecipient &info, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveRequestDialog),
    model(0),
    info(info)
{
    ui->setupUi(this);

    QString target = info.label;
    if(target.isEmpty())
        target = info.address;
    setWindowTitle(tr("Request payment to %1").arg(target));

    ui->lnAddress->setText(info.address);
    if(info.amount)
        ui->lnReqAmount->setValue(info.amount);
    ui->lnReqAmount->setReadOnly(true);
    ui->lnLabel->setText(info.label);
    ui->lnMessage->setText(info.message);

#ifndef USE_QRCODE
    ui->btnSaveAs->setVisible(false);
    ui->lblQRCode->setVisible(false);
#endif

    connect(ui->btnSaveAs, SIGNAL(clicked()), ui->lblQRCode, SLOT(saveImage()));

    genCode();
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if (model)
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void ReceiveRequestDialog::genCode()
{
    QString uri = getURI();
    ui->btnSaveAs->setEnabled(false);
    ui->outUri->setPlainText(uri);
#ifdef USE_QRCODE
    if (uri != "")
    {
        ui->lblQRCode->setText("");

        QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
        if (!code)
        {
            ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
            return;
        }
        QImage myImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        myImage.fill(0xffffff);
        unsigned char *p = code->data;
        for (int y = 0; y < code->width; y++)
        {
            for (int x = 0; x < code->width; x++)
            {
                myImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                p++;
            }
        }
        QRcode_free(code);

        ui->lblQRCode->setPixmap(QPixmap::fromImage(myImage).scaled(300, 300));
        ui->btnSaveAs->setEnabled(true);
    }
#endif
}

QString ReceiveRequestDialog::getURI()
{
    QString ret = QString("bitcoin:%1").arg(info.address);
    int paramCount = 0;

    if (ui->lnReqAmount->validate())
    {
        // even if we allow a non BTC unit input in lnReqAmount, we generate the URI with BTC as unit (as defined in BIP21)
        ret += QString("?amount=%1").arg(BitcoinUnits::format(BitcoinUnits::BTC, ui->lnReqAmount->value()));
        paramCount++;
    }

    if (!ui->lnLabel->text().isEmpty())
    {
        QString lbl(QUrl::toPercentEncoding(ui->lnLabel->text()));
        ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
        paramCount++;
    }

    if (!ui->lnMessage->text().isEmpty())
    {
        QString msg(QUrl::toPercentEncoding(ui->lnMessage->text()));
        ret += QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
        paramCount++;
    }

    // limit URI length
    if (ret.length() > MAX_URI_LENGTH)
    {
        ui->lblQRCode->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        return QString("");
    }

    return ret;
}

void ReceiveRequestDialog::on_lnReqAmount_textChanged()
{
    genCode();
}

void ReceiveRequestDialog::on_lnLabel_textChanged()
{
    genCode();
}

void ReceiveRequestDialog::on_lnMessage_textChanged()
{
    genCode();
}

void ReceiveRequestDialog::updateDisplayUnit()
{
    if (model)
    {
        // Update lnReqAmount with the current unit
        ui->lnReqAmount->setDisplayUnit(model->getDisplayUnit());
    }
}
