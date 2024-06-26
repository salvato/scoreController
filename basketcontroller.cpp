/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include <QDir>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QScreen>

#include "basketcontroller.h"
#include "edit.h"
#include "button.h"
#include "radioButton.h"
#include "fileserver.h"
#include "generalsetupdialog.h"
#include "utility.h"


#define MAX_FAULS      99 // To be defined the behaviour after #fauls becomes greater then MAX_FAULS
#define MAX_PERIODS    99 // To be defined the behaviour after #periods becomes greater then MAX_PERIODS


/*!
 * \brief BasketController::BasketController
 * The constructor of the Basket Control Panel.
 *
 * It is responsible to start the various services for
 * updating Slide and Spots
 */
BasketController::BasketController(QString sMyLanguage)
    : ScoreController(BASKET_PANEL, Q_NULLPTR)
    , bFontBuilt(false)
{
    sLanguage = sMyLanguage;
    GetSettings();
    prepareDirectories();

    logFileName = QString("%1score_controller.txt").arg(sLogDir);
    prepareLogFile();
    prepareServices();

    pSlideUpdaterServer->setDir(sSlideDir,"*.jpg *.jpeg *.png *.JPG *.JPEG *.PNG");
    pSpotUpdaterServer->setDir(sSpotDir, "*.mp4 *.MP4");

    emit startSpotServer();
    emit startSlideServer();

    buildControls();
    auto *mainLayout = new QGridLayout();

    int gamePanelWidth   = 15;
    int gamePanelHeight  = 8;

    mainLayout->addLayout(CreateGamePanel(),
                          0,
                          0,
                          gamePanelHeight,
                          gamePanelWidth);

    mainLayout->addLayout(CreateGameButtonBox(),
                          gamePanelHeight,
                          0,
                          2,
                          5);

    mainLayout->addLayout(CreateSpotButtons(),
                          gamePanelHeight,
                          5,
                          2,
                          gamePanelWidth-5);
    setLayout(mainLayout);

    setEventHandlers();

    possess[iPossess ? 1 : 0]->setChecked(true);
    possess[iPossess ? 0 : 1]->setChecked(false);
}


/*!
 * \brief BasketController::buildControls
 * Utility member to create all the controls on the Control Panel
 */
void
BasketController::buildControls() {
    QString sString;
    QPixmap plusPixmap, minusPixmap;
    QIcon plusButtonIcon, minusButtonIcon;
    plusPixmap.load(":/buttonIcons/Plus.png");
    plusButtonIcon.addPixmap(plusPixmap);
    minusPixmap.load(":/buttonIcons/Minus.png");
    minusButtonIcon.addPixmap(minusPixmap);
    for(int iTeam=0; iTeam<2; iTeam++) {
        // Teams
        teamName[iTeam] = new Edit(sTeam[iTeam], iTeam);
        teamName[iTeam]->setAlignment(Qt::AlignHCenter);
        teamName[iTeam]->setMaxLength(15);
        // Timeout
        sString = QString("%1").arg(iTimeout[iTeam]);
        timeoutEdit[iTeam] = new Edit(sString);
        timeoutEdit[iTeam]->setMaxLength(1);
        timeoutEdit[iTeam]->setAlignment(Qt::AlignHCenter);
        timeoutEdit[iTeam]->setReadOnly(true);
        // Timeout buttons
        timeoutIncrement[iTeam] = new Button("", iTeam);
        timeoutIncrement[iTeam]->setIcon(plusButtonIcon);
        timeoutIncrement[iTeam]->setIconSize(plusPixmap.rect().size());
        timeoutDecrement[iTeam] = new Button("", iTeam);
        timeoutDecrement[iTeam]->setIcon(minusButtonIcon);
        timeoutDecrement[iTeam]->setIconSize(minusPixmap.rect().size());
        if(iTimeout[iTeam] == 0)
            timeoutDecrement[iTeam]->setEnabled(false);
        // Team Fauls
        sString = QString("%1").arg(iFauls[iTeam]);
        faulsEdit[iTeam] = new Edit(sString);
        faulsEdit[iTeam]->setMaxLength(2);
        faulsEdit[iTeam]->setAlignment(Qt::AlignHCenter);
        faulsEdit[iTeam]->setReadOnly(true);
        // Team Fauls buttons
        faulsIncrement[iTeam] = new Button("", iTeam);
        faulsIncrement[iTeam]->setIcon(plusButtonIcon);
        faulsIncrement[iTeam]->setIconSize(plusPixmap.rect().size());
        faulsDecrement[iTeam] = new Button("", iTeam);
        faulsDecrement[iTeam]->setIcon(minusButtonIcon);
        faulsDecrement[iTeam]->setIconSize(minusPixmap.rect().size());
        if(iFauls[iTeam] == 0)
            faulsDecrement[iTeam]->setEnabled(false);
        if(iFauls[iTeam] == MAX_FAULS)
            faulsIncrement[iTeam]->setEnabled(false);
        // Possess
        possess[iTeam] = new RadioButton(" ", iTeam);
        // Bonus
        bonusEdit[iTeam] = new Edit(QString(tr("Bonus")));
        bonusEdit[iTeam]->setMaxLength(QString(tr("Bonus")).length());
        bonusEdit[iTeam]->setFrame(false);
        bonusEdit[iTeam]->setAlignment(Qt::AlignHCenter);
        bonusEdit[iTeam]->setReadOnly(true);
        if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
            bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        else {
            bonusEdit[iTeam]->setStyleSheet("background:transparent;color:transparent;");
        }
        // Score
        sString = QString("%1").arg(iScore[iTeam]);
        scoreEdit[iTeam] = new Edit(sString);
        scoreEdit[iTeam]->setMaxLength(3);
        scoreEdit[iTeam]->setReadOnly(true);
        scoreEdit[iTeam]->setAlignment(Qt::AlignRight);
        // Score buttons
        scoreIncrement[iTeam] = new Button("", iTeam);
        scoreIncrement[iTeam]->setIcon(plusButtonIcon);
        scoreIncrement[iTeam]->setIconSize(plusPixmap.rect().size());
        scoreDecrement[iTeam] = new Button("", iTeam);
        scoreDecrement[iTeam]->setIcon(minusButtonIcon);
        scoreDecrement[iTeam]->setIconSize(minusPixmap.rect().size());
        if(iScore[iTeam] == 0)
            scoreDecrement[iTeam]->setEnabled(false);
    }
    // Period
    sString = QString("%1").arg(iPeriod);
    periodEdit = new Edit(sString);
    periodEdit->setMaxLength(2);
    periodEdit->setReadOnly(true);
    periodEdit->setAlignment(Qt::AlignRight);
    // Period Buttons
    periodIncrement = new QPushButton("");
    periodIncrement->setIcon(plusButtonIcon);
    periodIncrement->setIconSize(plusPixmap.rect().size());
    periodDecrement = new QPushButton("");
    periodDecrement->setIcon(minusButtonIcon);
    periodDecrement->setIconSize(minusPixmap.rect().size());
    if(iPeriod < 2)
        periodDecrement->setDisabled(true);

    //Labels:

    // Timeout
    timeoutLabel = new QLabel(tr("Timeout"));
    timeoutLabel->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    // Team Fauls
    faulsLabel = new QLabel(tr("Falli Squadra"));
    faulsLabel->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    // Score
    scoreLabel = new QLabel(tr("Punteggio"));
    scoreLabel->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    // Period
    periodLabel = new QLabel(tr("Periodo"));
    periodLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    // Posses
    possessLabel = new QLabel(tr("Possesso"));
    possessLabel->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
}


/*!
 * \brief BasketController::resizeEvent
 * The very first time it is called it is resposible to resize the fonts used
 * \param event  unused
 */
void
BasketController::resizeEvent(QResizeEvent *event) {
    if(!bFontBuilt) {
        bFontBuilt = true;
        buildFontSizes();
        event->setAccepted(true);
    }
}


/*!
 * \brief BasketController::buildFontSizes
 * Utility member to calculate the font sizes for the various controls
 */
void
BasketController::buildFontSizes() {
    QFont font;
    int iFontSize;
    int hMargin, vMargin;
    QMargins margins;

    // Teams
    font = teamName[0]->font();
    margins = teamName[0]->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    font.setCapitalization(QFont::Capitalize);
    iFontSize = qMin((teamName[0]->width()/teamName[0]->maxLength())-hMargin,
                     teamName[0]->height()-vMargin);
    font.setPixelSize(iFontSize);
    teamName[0]->setFont(font);
    teamName[1]->setFont(font);
    // Timeout
    font = timeoutEdit[0]->font();
    margins = timeoutEdit[0]->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    iFontSize = qMin((timeoutEdit[0]->width()/timeoutEdit[0]->maxLength())-hMargin,
                     timeoutEdit[0]->height()-vMargin);
    font.setPixelSize(iFontSize);
    timeoutEdit[0]->setFont(font);
    timeoutEdit[1]->setFont(font);
    // Fauls
    font = faulsEdit[0]->font();
    margins = faulsEdit[0]->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    iFontSize = qMin((faulsEdit[0]->width()/faulsEdit[0]->maxLength())-hMargin,
                     faulsEdit[0]->height()-vMargin);
    font.setPixelSize(iFontSize);
    faulsEdit[0]->setFont(font);
    faulsEdit[1]->setFont(font);
    // Bonus
    font = bonusEdit[0]->font();
    margins = bonusEdit[0]->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    iFontSize = qMin((bonusEdit[0]->width()/bonusEdit[0]->maxLength())-hMargin,
                     bonusEdit[0]->height()-vMargin);
    font.setPixelSize(iFontSize);
    bonusEdit[0]->setFont(font);
    bonusEdit[1]->setFont(font);
    // Period
    font = periodEdit->font();
    margins = periodEdit->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    iFontSize = qMin((periodEdit->width()/periodEdit->maxLength())-hMargin,
                     periodEdit->height()-vMargin);
    font.setPixelSize(iFontSize);
    periodEdit->setFont(font);
    // Score
    font = scoreEdit[0]->font();
    margins = scoreEdit[0]->contentsMargins();
    vMargin = margins.bottom() + margins.top();
    hMargin = margins.left() + margins.right();
    font.setWeight(QFont::Black);
    iFontSize = qMin((scoreEdit[0]->width()/scoreEdit[0]->maxLength())-hMargin,
                     scoreEdit[0]->height()-vMargin);
    font.setPixelSize(iFontSize);
    scoreEdit[0]->setFont(font);
    scoreEdit[1]->setFont(font);

//Labels:
// Can't understand why it is not working
//    font = faulsLabel->font();
//    iFontSize = qMin(faulsLabel->width()/faulsLabel->text().length(),
//                     faulsLabel->height());
//    font.setPixelSize(iFontSize);
    faulsLabel->setFont(font);
    scoreLabel->setFont(font);
    font.setWeight(QFont::Normal);
    timeoutLabel->setFont(font);
    faulsLabel->setFont(font);
    possessLabel->setFont(font);
    periodLabel->setFont(font);
}


/*!
 * \brief BasketController::setEventHandlers
 * Utility member to connect the various controls with their event handlers
 */
void
BasketController::setEventHandlers() {
    for(int iTeam=0; iTeam<2; iTeam++) {
        // Team
        connect(teamName[iTeam], SIGNAL(textChanged(QString,int)),
                this, SLOT(onTeamTextChanged(QString,int)));
        // Timeout
        connect(timeoutIncrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onTimeOutIncrement(int)));
        connect(timeoutDecrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onTimeOutDecrement(int)));
        // Fauls
        connect(faulsIncrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onFaulsIncrement(int)));
        connect(faulsDecrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onFaulsDecrement(int)));
        // Possess
        connect(possess[iTeam], SIGNAL(buttonClicked(int,bool)),
                this, SLOT(onPossessClicked(int,bool)));
        // Score
        connect(scoreIncrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onScoreIncrement(int)));
        connect(scoreDecrement[iTeam], SIGNAL(buttonClicked(int)),
                this, SLOT(onScoreDecrement(int)));
    }
    // Period
    connect(periodIncrement, SIGNAL(clicked()),
            this, SLOT(onIncrementPeriod()));
    connect(periodDecrement, SIGNAL(clicked()),
            this, SLOT(onDecrementPeriod()));
    // Buttons
    connect(newPeriodButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonNewPeriodClicked()));
    connect(newGameButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonNewGameClicked()));
    connect(changeFieldButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonChangeFieldClicked()));
}


/*!
 * \brief BasketController::GetSettings
 * Recall from the non volatile memory the last values of the controls
 */
void
BasketController::GetSettings() {
    pSettings = new QSettings("Gabriele Salvato", "Basket Controller");

    sTeam[0]    = pSettings->value("team1/name", QString(tr("Locali"))).toString();
    sTeam[1]    = pSettings->value("team2/name", QString(tr("Ospiti"))).toString();
    iTimeout[0] = pSettings->value("team1/timeouts", 0).toInt();
    iTimeout[1] = pSettings->value("team2/timeouts", 0).toInt();
    iScore[0]   = pSettings->value("team1/score", 0).toInt();
    iScore[1]   = pSettings->value("team2/score", 0).toInt();
    iFauls[0]   = pSettings->value("team1/fauls", 0).toInt();
    iFauls[1]   = pSettings->value("team2/fauls", 0).toInt();

    iPeriod     = pSettings->value("game/period", 1).toInt();
    iPossess    = pSettings->value("game/possess", 0).toInt();

    sSlideDir   = pSettings->value("directories/slides", sSlideDir).toString();
    sSpotDir    = pSettings->value("directories/spots", sSpotDir).toString();

    for(int iTeam=0; iTeam<2; iTeam++) {
        if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
            iBonus[iTeam] = 1;
        }
        else {
            iBonus[iTeam] = 0;
        }
    }
}


/*!
 * \brief BasketController::closeEvent
 * To handle the closure of the Panel Controller
 * \param event
 */
void
BasketController::closeEvent(QCloseEvent *event) {
    SaveStatus();
    ScoreController::closeEvent(event);// Propagate the event
}


/*!
 * \brief BasketController::SaveStatus
 * Save the values of the controls into the non volatile memory
 */
void
BasketController::SaveStatus() {
    pSettings->setValue("team1/name", sTeam[0]);
    pSettings->setValue("team2/name", sTeam[1]);
    pSettings->setValue("team1/timeouts", iTimeout[0]);
    pSettings->setValue("team2/timeouts", iTimeout[1]);
    pSettings->setValue("team1/score", iScore[0]);
    pSettings->setValue("team2/score", iScore[1]);
    pSettings->setValue("team1/fauls", iFauls[0]);
    pSettings->setValue("team2/fauls", iFauls[1]);

    pSettings->setValue("game/period", iPeriod);
    pSettings->setValue("game/possess", iPossess);

    pSettings->setValue("directories/slides", sSlideDir);
    pSettings->setValue("directories/spots", sSpotDir);
}


/*!
 * \brief BasketController::CreateGameButtonBox
 * Utility member to create the Layout of the Buttons
 * \return the Layout
 */
QHBoxLayout*
BasketController::CreateGameButtonBox() {
    auto *gameButtonLayout = new QHBoxLayout();
    QPixmap pixmap(":/buttonIcons/ExchangeBasketField.png");
    QIcon ButtonIcon(pixmap);

    changeFieldButton = new QPushButton(ButtonIcon, "");
    changeFieldButton->setIconSize(pixmap.rect().size());

    pixmap.load(":/buttonIcons/New-Set-Volley.png");
    ButtonIcon.addPixmap(pixmap);
    newPeriodButton   = new QPushButton(ButtonIcon, "");
    newPeriodButton->setIconSize(pixmap.rect().size());

    pixmap.load(":/buttonIcons/New-Game-Volley.png");
    ButtonIcon.addPixmap(pixmap);
    newGameButton = new QPushButton(ButtonIcon, "");
    newGameButton->setIconSize(pixmap.rect().size());

    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(newPeriodButton);
    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(newGameButton);
    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(changeFieldButton);
    gameButtonLayout->addStretch();
    return gameButtonLayout;
}


/*!
 * \brief BasketController::CreateGamePanel
 * Utility member to create the Layout of the Control Panel
 * \return The Layout
 */
QGridLayout*
BasketController::CreateGamePanel() {
    auto *gamePanel = new QGridLayout();
    //
    int iRow;
    for(int iTeam=0; iTeam<2; iTeam++) {
        // Matrice x righe e 8 colonne
        iRow = 0;
        gamePanel->addWidget(teamName[iTeam], iRow, iTeam*4, 1, 4);
        int iCol = iTeam*5;
        iRow += 1;
        gamePanel->addWidget(timeoutDecrement[iTeam], iRow, iCol,   1, 1, Qt::AlignRight);
        gamePanel->addWidget(timeoutEdit[iTeam],      iRow, iCol+1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);
        gamePanel->addWidget(timeoutIncrement[iTeam], iRow, iCol+2, 1, 1, Qt::AlignLeft);
        iRow += 1;
        gamePanel->addWidget(faulsDecrement[iTeam], iRow, iCol,   1, 1, Qt::AlignRight);
        gamePanel->addWidget(faulsEdit[iTeam],      iRow, iCol+1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);
        gamePanel->addWidget(faulsIncrement[iTeam], iRow, iCol+2, 1, 1, Qt::AlignLeft);
        iRow += 1;
        if(iTeam == 0) {
            gamePanel->addWidget(possess[iTeam],   iRow, 2, 1, 1, Qt::AlignRight|Qt::AlignVCenter);
        } else {
            gamePanel->addWidget(possess[iTeam],   iRow, 5, 1, 1, Qt::AlignLeft|Qt::AlignVCenter);
        }
        iRow += 2;// Leave space for Bonus & Period
        gamePanel->addWidget(scoreDecrement[iTeam], iRow, iCol,   2, 1, Qt::AlignRight);
        gamePanel->addWidget(scoreEdit[iTeam],      iRow, iCol+1, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
        gamePanel->addWidget(scoreIncrement[iTeam], iRow, iCol+2, 2, 1, Qt::AlignLeft);
    }
    gamePanel->addWidget(timeoutLabel,  1, 3, 1, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    gamePanel->addWidget(faulsLabel,    2, 3, 1, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    gamePanel->addWidget(possessLabel,  3, 3, 1, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    gamePanel->addWidget(bonusEdit[0],  3, 0, 1, 2, Qt::AlignLeft|Qt::AlignVCenter);
    gamePanel->addWidget(bonusEdit[1],  3, 6, 1, 2, Qt::AlignRight|Qt::AlignVCenter);

    gamePanel->addWidget(periodLabel,      4, 0, 1, 2, Qt::AlignRight|Qt::AlignVCenter);
    gamePanel->addWidget(periodDecrement,  4, 2, 1, 1, Qt::AlignRight);
    gamePanel->addWidget(periodEdit,       4, 3, 1, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    gamePanel->addWidget(periodIncrement,  4, 5, 1, 1, Qt::AlignLeft);

    gamePanel->addWidget(scoreLabel,       5, 3, 2, 2, Qt::AlignHCenter|Qt::AlignVCenter);

    return gamePanel;
}


/*!
 * \brief BasketController::FormatStatusMsg
 * Utility function to format a String containing the current status of the controls
 * \return The "Current Status Message"
 */
QString
BasketController::FormatStatusMsg() {
    QString sMessage = QString();
    QString sTemp;
    for(int i=0; i<2; i++) {
        sTemp = QString("<team%1>%2</team%3>").arg(i).arg(sTeam[i].toLocal8Bit().data()).arg(i);
        sMessage += sTemp;
        sTemp = QString("<timeout%1>%2</timeout%3>").arg(i).arg(iTimeout[i]).arg(i);
        sMessage += sTemp;
        sTemp = QString("<score%1>%2</score%3>").arg(i).arg(iScore[i]).arg(i);
        sMessage += sTemp;
        sTemp = QString("<fauls%1>%2</fauls%3>").arg(i).arg(iFauls[i]).arg(i);
        sMessage += sTemp;
        sTemp = QString("<bonus%1>%2</bonus%3>").arg(i).arg(iBonus[i]).arg(i);
        sMessage += sTemp;
    }
    if(iPeriod > pGeneralSetupDialog->getGamePeriodsBB())
        sTemp = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getOverTimeBB());
    else
        sTemp = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getRegularTimeBB());
    sMessage += sTemp;
    sTemp = QString("<possess>%1</possess>").arg(iPossess);
    sMessage += sTemp;
    if(myStatus == showSlides)
        sMessage += "<slideshow>1</slideshow>";
    else if(myStatus == showCamera)
        sMessage += QString("<live>1</live>");
    else if(myStatus == showSpots)
        sMessage += QString("<spotloop>1</spotloop>");

    sMessage += QString("<language>%1</language>").arg(sLanguage);
    return sMessage;
}


// =========================
// Event management routines
// =========================

/*!
 * \brief BasketController::onTimeOutIncrement
 * \param iTeam
 */
void
BasketController::onTimeOutIncrement(int iTeam) {
    QString sMessage;
    iTimeout[iTeam]++;
    if((iPeriod < 3) && (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout1BB())) {
        timeoutIncrement[iTeam]->setEnabled(false);
        timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    else if((iPeriod > pGeneralSetupDialog->getGamePeriodsBB()) &&
            (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout3BB())) {
        timeoutIncrement[iTeam]->setEnabled(false);
        timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    else if((iPeriod > 2) && (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout2BB())) {
        timeoutIncrement[iTeam]->setEnabled(false);
        timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    timeoutDecrement[iTeam]->setEnabled(true);
    sMessage = QString("<timeout%1>%2</timeout%3>").arg(iTeam).arg(iTimeout[iTeam]).arg(iTeam);
    SendToAll(sMessage);
    QString sText;
    sText = QString("%1").arg(iTimeout[iTeam]);
    timeoutEdit[iTeam]->setText(sText);
    sText = QString("team%1/timeouts").arg(iTeam+1);
    pSettings->setValue(sText, iTimeout[iTeam]);
}


/*!
 * \brief BasketController::onTimeOutDecrement
 * \param iTeam
 */
void
BasketController::onTimeOutDecrement(int iTeam) {
    QString sMessage;
    iTimeout[iTeam]--;
    if(iTimeout[iTeam] == 0) {
        timeoutDecrement[iTeam]->setEnabled(false);
    }
    timeoutEdit[iTeam]->setStyleSheet(styleSheet());
    timeoutIncrement[iTeam]->setEnabled(true);
    sMessage = QString("<timeout%1>%2</timeout%3>").arg(iTeam).arg(iTimeout[iTeam]).arg(iTeam);
    SendToAll(sMessage);
    QString sText;
    sText = QString("%1").arg(iTimeout[iTeam]);
    timeoutEdit[iTeam]->setText(sText);
    sText = QString("team%1/timeouts").arg(iTeam+1);
    pSettings->setValue(sText, iTimeout[iTeam]);
}


/*!
 * \brief BasketController::onFaulsIncrement
 * \param iTeam
 */
void
BasketController::onFaulsIncrement(int iTeam) {
    QString sMessage, sText;
    iFauls[iTeam]++;
    faulsDecrement[iTeam]->setEnabled(true);
    if(iFauls[iTeam] == MAX_FAULS) {// To be changed
        faulsIncrement[iTeam]->setEnabled(false);
    }
    if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
        iBonus[iTeam] = 1;
        bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    else {
        iBonus[iTeam] = 0;
        bonusEdit[iTeam]->setStyleSheet("background:transparent;color:transparent;");
    }
    faulsDecrement[iTeam]->setEnabled(true);

    sMessage = QString("<fauls%1>%2</fauls%3>").arg(iTeam).arg(iFauls[iTeam]).arg(iTeam);
    sText = QString("<bonus%1>%2</bonus%3>").arg(iTeam).arg(iBonus[iTeam]).arg(iTeam);
    sMessage += sText;
    SendToAll(sMessage);
    sText = QString("%1").arg(iFauls[iTeam]);
    faulsEdit[iTeam]->setText(sText);
    sText = QString("team%1/fauls").arg(iTeam+1);
    pSettings->setValue(sText, iFauls[iTeam]);
}


/*!
 * \brief BasketController::onFaulsDecrement
 * \param iTeam
 */
void
BasketController::onFaulsDecrement(int iTeam) {
    QString sMessage, sText;
    iFauls[iTeam]--;
    faulsIncrement[iTeam]->setEnabled(true);
    if(iFauls[iTeam] == 0) {
       faulsDecrement[iTeam]->setEnabled(false);
    }
    if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
        iBonus[iTeam] = 1;
        bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    else {
        iBonus[iTeam] = 0;
        bonusEdit[iTeam]->setStyleSheet("background:transparent;color:transparent;");
    }
    faulsIncrement[iTeam]->setEnabled(true);

    sMessage = QString("<fauls%1>%2</fauls%3>").arg(iTeam).arg(iFauls[iTeam]).arg(iTeam);
    sText = QString("<bonus%1>%2</bonus%3>").arg(iTeam).arg(iBonus[iTeam]).arg(iTeam);
    sMessage += sText;
    SendToAll(sMessage);
    sText = QString("%1").arg(iFauls[iTeam]);
    faulsEdit[iTeam]->setText(sText);
    sText = QString("team%1/fauls").arg(iTeam+1);
    pSettings->setValue(sText, iFauls[iTeam]);
}


/*!
 * \brief BasketController::onScoreIncrement
 * \param iTeam
 */
void
BasketController::onScoreIncrement(int iTeam) {
    QString sMessage;
    iScore[iTeam]++;
    scoreDecrement[iTeam]->setEnabled(true);
    if(iScore[iTeam] > 998) {
        scoreIncrement[iTeam]->setEnabled(false);
    }
    sMessage = QString("<score%1>%2</score%3>").arg(iTeam).arg(iScore[iTeam]).arg(iTeam);
    SendToAll(sMessage);
    QString sText;
    sText = QString("%1d").arg(iScore[iTeam]);
    scoreEdit[iTeam]->setText(sText);
    sText = QString("team%1/score").arg(iTeam+1);
    pSettings->setValue(sText, iScore[iTeam]);
}


/*!
 * \brief BasketController::onScoreDecrement
 * \param iTeam
 */
void
BasketController::onScoreDecrement(int iTeam) {
    QString sMessage;
    iScore[iTeam]--;
    scoreIncrement[iTeam]->setEnabled(true);
    if(iScore[iTeam] == 0) {
        scoreDecrement[iTeam]->setEnabled(false);
    }
    sMessage = QString("<score%1>%2</score%3>").arg(iTeam).arg(iScore[iTeam]).arg(iTeam);
    SendToAll(sMessage);
    QString sText;
    sText = QString("%1").arg(iScore[iTeam]);
    scoreEdit[iTeam]->setText(sText);
    sText = QString("team%1/score").arg(iTeam+1);
    pSettings->setValue(sText, iScore[iTeam]);
}


/*!
 * \brief BasketController::onTeamTextChanged
 * \param sText
 * \param iTeam
 */
void
BasketController::onTeamTextChanged(QString sText, int iTeam) {
    QString sMessage;
    sTeam[iTeam] = sText;
    if(sText=="")// C'è un problema con la stringa vuota...
        sMessage = QString("<team%1>-</team%2>").arg(iTeam).arg(iTeam);
    else
        sMessage = QString("<team%1>%2</team%3>").arg(iTeam).arg(sTeam[iTeam].toLocal8Bit().data()).arg(iTeam);
    SendToAll(sMessage);
    sText = QString("team%1/name").arg(iTeam+1);
    pSettings->setValue(sText, sTeam[iTeam]);
}


/*!
 * \brief BasketController::onButtonChangeFieldClicked
 */
void
BasketController::onButtonChangeFieldClicked() {
    int iRes = QMessageBox::question(this, tr("BasketController"),
                                     tr("Scambiare il campo delle squadre ?"),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    if(iRes != QMessageBox::Yes) return;

    // Exchange teams order, score, timeouts and team fauls
    QString sText = sTeam[0];
    sTeam[0] = sTeam[1];
    sTeam[1] = sText;

    int iVal = iScore[0];
    iScore[0] = iScore[1];
    iScore[1] = iVal;

    iVal = iTimeout[0];
    iTimeout[0] = iTimeout[1];
    iTimeout[1] = iVal;

    iVal = iFauls[0];
    iFauls[0] = iFauls[1];
    iFauls[1] = iVal;
    // Update panel
    for(int iTeam=0; iTeam<2; iTeam++) {
        teamName[iTeam]->setText(sTeam[iTeam]);
        sText = QString("%1").arg(iScore[iTeam]);
        scoreEdit[iTeam]->setText(sText);
        scoreDecrement[iTeam]->setEnabled(true);
        scoreIncrement[iTeam]->setEnabled(true);
        if(iScore[iTeam] == 0) {
          scoreDecrement[iTeam]->setEnabled(false);
        }
        if(iScore[iTeam] > 98) {
          scoreIncrement[iTeam]->setEnabled(false);
        }
        sText = QString("%1").arg(iTimeout[iTeam]);
        timeoutEdit[iTeam]->setText(sText);
        timeoutIncrement[iTeam]->setEnabled(true);
        timeoutDecrement[iTeam]->setEnabled(true);
        timeoutEdit[iTeam]->setStyleSheet(styleSheet());
        if((iPeriod < 3) && (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout1BB())) {
            timeoutIncrement[iTeam]->setEnabled(false);
            timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        else if((iPeriod > pGeneralSetupDialog->getGamePeriodsBB()) &&
                (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout3BB())) {
            timeoutIncrement[iTeam]->setEnabled(false);
            timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        else if((iPeriod > 2) && (iTimeout[iTeam] == pGeneralSetupDialog->getNumTimeout2BB())) {
            timeoutIncrement[iTeam]->setEnabled(false);
            timeoutEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        if(iTimeout[iTeam] == 0) {
            timeoutDecrement[iTeam]->setEnabled(false);
        }
        sText = QString("%1").arg(iFauls[iTeam]);
        faulsEdit[iTeam]->setText(sText);
        if(iFauls[iTeam] == 0) {
           faulsDecrement[iTeam]->setEnabled(false);
        }
        if(iFauls[iTeam] == MAX_FAULS) {// To be changed
            faulsIncrement[iTeam]->setEnabled(false);
        }
        if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
            iBonus[iTeam] = 1;
            bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        else {
            iBonus[iTeam] = 0;
            bonusEdit[iTeam]->setStyleSheet("background:transparent;color:transparent;");
        }
    }
    SendToAll(FormatStatusMsg());
    SaveStatus();
}


/*!
 * \brief BasketController::onButtonNewPeriodClicked
 */
void
BasketController::onButtonNewPeriodClicked() {
    int iRes = QMessageBox::question(this, tr("BasketController"),
                                     tr("Vuoi davvero iniziare un nuovo Periodo ?"),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    if(iRes != QMessageBox::Yes) return;

    // Increment period number
    if(iPeriod < MAX_PERIODS) {
        iPeriod++;
    }
    if(iPeriod >= MAX_PERIODS) {
        periodIncrement->setDisabled(true);
        iPeriod= MAX_PERIODS;
    }
    periodDecrement->setEnabled(true);
    QString sString;
    sString = QString("%1").arg(iPeriod);
    periodEdit->setText(sString);

    // Exchange teams order, score and team fauls
    QString sText = sTeam[0];
    sTeam[0] = sTeam[1];
    sTeam[1] = sText;
    int iVal = iScore[0];
    iScore[0] = iScore[1];
    iScore[1] = iVal;
    if(iPeriod > pGeneralSetupDialog->getGamePeriodsBB()) {// Art. 41.1.3 - Tutti i falli di squadra commessi in un tempo
                                //supplementare devono essere considerati come avvenuti nel quarto periodo.
        iVal = iFauls[0];
        iFauls[0] = iFauls[1];
        iFauls[1] = iVal;
     }
    else {
        iFauls[0] = iFauls[1] = 0;
    }
    // Update panel
    for(int iTeam=0; iTeam<2; iTeam++) {
        teamName[iTeam]->setText(sTeam[iTeam]);
        iTimeout[iTeam] = 0;
        sText = QString("%1").arg(iTimeout[iTeam]);
        timeoutEdit[iTeam]->setText(sText);
        timeoutEdit[iTeam]->setStyleSheet(styleSheet());
        timeoutDecrement[iTeam]->setEnabled(false);
        timeoutIncrement[iTeam]->setEnabled(true);
        sText = QString("%1").arg(iScore[iTeam]);
        scoreEdit[iTeam]->setText(sText);
        sText = QString("%1").arg(iFauls[iTeam]);
        faulsEdit[iTeam]->setText(sText);
        if(iFauls[iTeam] == 0) {
           faulsDecrement[iTeam]->setEnabled(false);
        }
        if(iFauls[iTeam] == MAX_FAULS) {// To be changed
            faulsIncrement[iTeam]->setEnabled(false);
        }
        if(iFauls[iTeam] < pGeneralSetupDialog->getBonusTargetBB()) {
            iBonus[iTeam] = 1;
            bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
        }
        else {
            iBonus[iTeam] = 0;
            bonusEdit[iTeam]->setStyleSheet("background:transparent;color:transparent;");
        }
    }
    SendToAll(FormatStatusMsg());
    SaveStatus();
}


/*!
 * \brief BasketController::onButtonNewGameClicked
 */
void
BasketController::onButtonNewGameClicked() {
    int iRes = QMessageBox::question(this, tr("BasketController"),
                                     tr("Iniziare una Nuova Partita ?"),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    if(iRes != QMessageBox::Yes) return;
    sTeam[0]    = tr("Locali");
    sTeam[1]    = tr("Ospiti");
    QString sText;
    iPeriod = 1;
    sText = QString("%1").arg(iPeriod);
    periodEdit->setText(sText);
    periodIncrement->setEnabled(true);
    periodDecrement->setEnabled(false);
    for(int iTeam=0; iTeam<2; iTeam++) {
        teamName[iTeam]->setText(sTeam[iTeam]);
        iTimeout[iTeam] = 0;
        sText = QString("%1").arg(iTimeout[iTeam]);
        timeoutEdit[iTeam]->setText(sText);
        timeoutEdit[iTeam]->setStyleSheet(styleSheet());
        timeoutDecrement[iTeam]->setEnabled(false);
        timeoutIncrement[iTeam]->setEnabled(true);
        iScore[iTeam]   = 0;
        sText = QString("%1").arg(iScore[iTeam]);
        scoreEdit[iTeam]->setText(sText);
        scoreDecrement[iTeam]->setEnabled(false);
        scoreIncrement[iTeam]->setEnabled(true);
        iFauls[iTeam] = 0;
        sText = QString("%1").arg(iFauls[iTeam]);
        faulsEdit[iTeam]->setText(sText);
        faulsIncrement[iTeam]->setEnabled(true);
        faulsDecrement[iTeam]->setEnabled(false);
        bonusEdit[iTeam]->setStyleSheet("background:red;color:white;");
    }
    SendToAll(FormatStatusMsg());
    SaveStatus();
}


/*!
 * \brief BasketController::onIncrementPeriod
 */
void
BasketController::onIncrementPeriod() {
    if(iPeriod < MAX_PERIODS) {
        iPeriod++;
    }
    else if(iPeriod >= MAX_PERIODS) {
        periodIncrement->setDisabled(true);
        iPeriod= MAX_PERIODS;
    }
    periodDecrement->setEnabled(true);
    QString sString, sMessage;
    sString = QString("%1").arg(iPeriod);
    periodEdit->setText(sString);
    if(iPeriod > pGeneralSetupDialog->getGamePeriodsBB())
        sMessage = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getOverTimeBB());
    else
        sMessage = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getRegularTimeBB());
    SendToAll(sMessage);
    pSettings->setValue("game/period", iPeriod);
}


/*!
 * \brief BasketController::onDecrementPeriod
 */
void
BasketController::onDecrementPeriod() {
    if(iPeriod > 1) {
        iPeriod--;
    }
    else if(iPeriod < 2)
        periodDecrement->setDisabled(true);
    if(iPeriod >= MAX_PERIODS) {
        periodIncrement->setDisabled(true);
        iPeriod= MAX_PERIODS;
    }
    periodIncrement->setEnabled(true);
    QString sString, sMessage;
    sString = QString("%1").arg(iPeriod);
    periodEdit->setText(sString);
    if(iPeriod > pGeneralSetupDialog->getGamePeriodsBB())
        sMessage = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getOverTimeBB());
    else
        sMessage = QString("<period>%1,%2</period>").arg(iPeriod).arg(pGeneralSetupDialog->getRegularTimeBB());
    SendToAll(sMessage);
    pSettings->setValue("game/period", iPeriod);
}


/*!
 * \brief BasketController::onPossessClicked
 * \param iTeam
 * \param bChecked
 */
void
BasketController::onPossessClicked(int iTeam, bool bChecked) {
    Q_UNUSED(bChecked)
    QString sMessage;
    iPossess = iTeam;
    possess[iPossess ? 1 : 0]->setChecked(true);
    possess[iPossess ? 0 : 1]->setChecked(false);
    sMessage = QString("<possess>%1</possess>").arg(iPossess);
    SendToAll(sMessage);
    pSettings->setValue("game/possess", iPossess);
}


