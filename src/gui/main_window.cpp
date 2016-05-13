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
    along with CutVideo. If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/main_window.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFileDialog>

extern "C" {
#include "media/ffmpeg_utils.h"
#include "media/codec_holder.h"
}

const int width = 640;
const int height = 480;

MainWindow::MainWindow()
  : QMainWindow() {
  setWindowTitle(PROJECT_NAME_TITLE " " PROJECT_VERSION);
  QWidget *widget = new QWidget;
  setCentralWidget(widget);

  QHBoxLayout* mainl = new QHBoxLayout;
  selectFileButton_ = new QPushButton;
  selectFileButton_->setText("...");
  connect(selectFileButton_, &QPushButton::clicked, this, &MainWindow::openVideoFile);
  filePath_ = new QLineEdit;
  filePath_->setEnabled(false);
  mainl->addWidget(filePath_);
  mainl->addWidget(selectFileButton_);

  widget->setLayout(mainl);
}

void MainWindow::openVideoFile() {
  QString filepathR = QFileDialog::getOpenFileName(this, tr("Select video file"),
                                                     QString(),
                                                     tr("Video files (*.mp4)"));
  if (filepathR.isNull()) {
    return;
  }

  filePath_->setText(filepathR);

  QByteArray input_filename = filepathR.toLocal8Bit();
  AVFormatContext *fmt_ctx = NULL;
  int result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
  if (result < 0) {
    debug_av_perror("avformat_open_input", result);
    return;
  }

  int video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream < 0) {
    debug_av_perror("av_find_best_stream", video_stream);
    return;
  }

  AVCodecContext *origin_ctx = fmt_ctx->streams[video_stream]->codec;
  decoder_t* dec = alloc_decoder_by_ctx(origin_ctx);
  if (!dec) {
    return;
  }

  AVFrame* fr = av_frame_alloc();
  if (!fr) {
    debug_av_perror("av_frame_alloc", video_stream);
    return;
  }

  int end_of_stream = 0;
  int got_frame = 0;
  int i = 0;
  AVPacket pkt;
  av_init_packet(&pkt);
  do {
    if (!end_of_stream) {
      if (av_read_frame(fmt_ctx, &pkt) < 0) {
        end_of_stream = 1;
      }
    }

    if (end_of_stream) {
      pkt.data = NULL;
      pkt.size = 0;
    }
    if (pkt.stream_index == video_stream || end_of_stream) {
      got_frame = 0;
      if (pkt.pts == AV_NOPTS_VALUE) {
        pkt.pts = pkt.dts = i;
      }
      int result = decoder_decode_video(dec, fr, &pkt);
      if (result < 0) {
        break;
      }
      av_packet_unref(&pkt);
      av_init_packet(&pkt);
    }
    i++;
  } while (!end_of_stream || got_frame);

  av_packet_unref(&pkt);
  av_frame_free(&fr);
  free_decoder(dec);
  avformat_close_input(&fmt_ctx);
}
