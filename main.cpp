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

//#include "scorecontroller.h"
#include "volleycontroller.h"
#include "basketcontroller.h"
#include "choosediscilpline.h"
#include <QApplication>
#include <QMessageBox>


int
main(int argc, char *argv[]) {

    QApplication app(argc, argv);

//    QFont myFont = QApplication::font();
//    myFont.setPointSize(32);
//    app.setFont(myFont, "QEdit");
//    myFont.setPointSize(18);
//    app.setFont(myFont, "QRadioButton");
//    app.setFont(myFont, "QLabel");

    ChooseDiscilpline chooser;
    chooser.exec();

    ScoreController* pController;
    if(chooser.getDiscipline() == BASKET_PANEL)
        pController = new BasketController();
    else
        pController = new VolleyController();

#ifdef Q_OS_ANDROID
    pController->showFullScreen();
#else
    pController->showMaximized();
    //pController->show();
#endif

    int iresult = app.exec();
    return iresult;
}
