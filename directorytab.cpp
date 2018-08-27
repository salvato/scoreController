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
#include <QFileDialog>
#include <QLabel>
#include <QGridLayout>
#include <QStandardPaths>

#include "directorytab.h"
#include <utility>

/*!
 * \brief DirectoryTab::DirectoryTab
 * \param parent
 */
DirectoryTab::DirectoryTab(QWidget *parent)
    : QWidget(parent)
{
    slidesDirEdit.setText(QString(""));
    spotsDirEdit.setText(QString(""));
    slidesDirEdit.setStyleSheet("background:red;color:white;");
    spotsDirEdit.setStyleSheet("background:red;color:white;");

    buttonSelectSlidesDir.setText("...");
    buttonSelectSpotsDir.setText("...");
    connect(&buttonSelectSlidesDir, SIGNAL(clicked()),
            this, SLOT(onSelectSlideDir()));
    connect(&buttonSelectSpotsDir, SIGNAL(clicked()),
            this, SLOT(onSelectSpotDir()));

    slidesDirEdit.setReadOnly(true);
    spotsDirEdit.setReadOnly(true);

    QLabel *slidesPathLabel = new QLabel(tr("Slides folder:"));
    QLabel *spotsPathLabel = new QLabel(tr("Spots folder:"));

    auto *mainLayout = new QGridLayout;
    mainLayout->addWidget(slidesPathLabel,        0, 0, 1, 1);
    mainLayout->addWidget(&slidesDirEdit,         0, 1, 1, 3);
    mainLayout->addWidget(&buttonSelectSlidesDir, 0, 4, 1, 1);

    mainLayout->addWidget(spotsPathLabel,        1, 0, 1, 1);
    mainLayout->addWidget(&spotsDirEdit,         1, 1, 1, 3);
    mainLayout->addWidget(&buttonSelectSpotsDir, 1, 4, 1, 1);

    setLayout(mainLayout);
}


void
DirectoryTab::setSlideDir(const QString& sDir) {
    slidesDirEdit.setText(sDir);
    QDir slideDir(sDir);
    if(slideDir.exists()) {
        slidesDirEdit.setStyleSheet(styleSheet());
    }
    else {
        slidesDirEdit.setStyleSheet("background:red;color:white;");
    }
}


void
DirectoryTab::setSpotDir(const QString& sDir){
    spotsDirEdit.setText(sDir);
    QDir spotDir(sDir);
    if(spotDir.exists()) {
        spotsDirEdit.setStyleSheet(styleSheet());
    }
    else {
        spotsDirEdit.setStyleSheet("background:red;color:white;");
    }
}


QString
DirectoryTab::getSlideDir() {
    return slidesDirEdit.text();
}


QString
DirectoryTab::getSpotDir(){
    return spotsDirEdit.text();
}


void
DirectoryTab::onSelectSlideDir() {
    QString sSlideDir = slidesDirEdit.text();
    QFileDialog* pGetDirDlg;
    QDir slideDir = QDir(sSlideDir);
    if(slideDir.exists()) {
        pGetDirDlg = new QFileDialog(this, tr("Slide Dir"),
                                     sSlideDir);
    }
    else {
        pGetDirDlg = new QFileDialog(this, tr("Slide Dir"),
                                     QStandardPaths::displayName(QStandardPaths::GenericDataLocation));
    }
    pGetDirDlg->setOptions(QFileDialog::ShowDirsOnly);
    pGetDirDlg->setFileMode(QFileDialog::Directory);
    pGetDirDlg->setViewMode(QFileDialog::List);
#ifdef Q_OS_ANDROID
    pGetDirDlg->setWindowFlags(Qt::Window);
#endif
    if(pGetDirDlg->exec() == QDialog::Accepted)
        sSlideDir = pGetDirDlg->directory().absolutePath();
    pGetDirDlg->deleteLater();
    if(!sSlideDir.endsWith(QString("/"))) sSlideDir+= QString("/");
    slidesDirEdit.setText(sSlideDir);
    slideDir.setPath(sSlideDir);
    if(slideDir.exists()) {
        slidesDirEdit.setStyleSheet(styleSheet());
    }
    else {
        slidesDirEdit.setStyleSheet("background:red;color:white;");
    }
}


void
DirectoryTab::onSelectSpotDir() {
    QString sSpotDir = spotsDirEdit.text();
    QFileDialog* pGetDirDlg;
    QDir spotDir = QDir(sSpotDir);
    if(spotDir.exists()) {
        pGetDirDlg = new QFileDialog(this, tr("Spot Dir"),
                                     sSpotDir);
    }
    else {
        pGetDirDlg = new QFileDialog(this, tr("Spot Dir"),
                                     QStandardPaths::displayName(QStandardPaths::GenericDataLocation));
    }
    pGetDirDlg->setOptions(QFileDialog::ShowDirsOnly);
    pGetDirDlg->setFileMode(QFileDialog::Directory);
    pGetDirDlg->setViewMode(QFileDialog::List);
#ifdef Q_OS_ANDROID
    pGetDirDlg->setWindowFlags(Qt::Window);
#endif
    if(pGetDirDlg->exec() == QDialog::Accepted)
        sSpotDir = pGetDirDlg->directory().absolutePath();
    pGetDirDlg->deleteLater();
    if(!sSpotDir.endsWith(QString("/"))) sSpotDir+= QString("/");
    spotsDirEdit.setText(sSpotDir);
    spotDir.setPath(sSpotDir);
    if(spotDir.exists()) {
        spotsDirEdit.setStyleSheet(styleSheet());
    }
    else {
        spotsDirEdit.setStyleSheet("background:red;color:white;");
    }
}
