/*  Copyright (C) 2014-2016 Alexandr Topilski. All right reserved.

    This file is part of CutVideo.

    CutVideo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CutVideo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CutVideo.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QApplication>
#include <QDesktopWidget>

#include "gui/main_window.h"

namespace {
  const QSize preferedSize(1024, 768);
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setOrganizationName(PROJECT_COMPANYNAME);
  app.setApplicationName(PROJECT_NAME);
  app.setApplicationVersion(PROJECT_VERSION);
  app.setAttribute(Qt::AA_UseHighDpiPixmaps);

  MainWindow win;
  QRect screenGeometry = app.desktop()->availableGeometry();
  QSize screenSize(screenGeometry.width(), screenGeometry.height());

#ifdef OS_ANDROID
  win.resize(screenSize);
#else
  QSize size(screenGeometry.width()/2, screenGeometry.height()/2);
  if (preferedSize.height() <= screenSize.height() && preferedSize.width() <= screenSize.width()) {
    win.resize(preferedSize);
  } else {
    win.resize(size);
  }

  QPoint center = screenGeometry.center();
  win.move(center.x() - win.width() * 0.5, center.y() - win.height() * 0.5);
#endif

  win.show();
  return app.exec();
}
