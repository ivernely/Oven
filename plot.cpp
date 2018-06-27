#include "mainwindow.h"
#include "ui_mainwindow.h"

//--------------------------------------------------------------
void MainWindow::SetPlotArea(QCustomPlot *widget)
{
    QPen    gPen;

    widget->clearGraphs();
    widget->setLocale(QLocale::system());

    widget->addGraph(widget->xAxis, widget->yAxis);         //sample temperature      
    gPen = widget->graph(0)->pen();
    gPen.setWidth(2);
    gPen.setColor(QColor(0, 255, 0));                       //green line
    widget->graph(0)->setPen(gPen);
    widget->graph(0)->setVisible(true);
    widget->graph(0)->setLineStyle(QCPGraph::lsLine);
    widget->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QColor(0, 255, 0),QColor(0, 255, 0),10));
    widget->graph(0)->setName(trUtf8("Тобразца"));

    widget->addGraph(widget->xAxis, widget->yAxis);         //water temperature
    gPen.setColor(QColor(0, 0, 255));                       //blue line
    widget->graph(1)->setPen(gPen);
    widget->graph(1)->setVisible(true);
    widget->graph(1)->setLineStyle(QCPGraph::lsLine);
    widget->graph(1)->setName(trUtf8("Тводы"));

    widget->addGraph(widget->xAxis, widget->yAxis2);        //rain intensivity
    gPen.setColor(QColor(255, 0, 0));                       //red line
    widget->graph(2)->setPen(gPen);
    widget->graph(2)->setVisible(true);
    widget->graph(2)->setLineStyle(QCPGraph::lsLine);
    widget->graph(2)->setName(trUtf8("Интенс.осадк."));

    widget->yAxis2->setVisible(true);
    widget->yAxis->setLabel(trUtf8("Температура, %1С").arg(QChar(176)));
    widget->yAxis2->setLabel(trUtf8("Интенсивн. осадков, мм/мин"));
    widget->xAxis->setLabel(trUtf8("Время, мин"));

    widget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(widget->xAxis, SIGNAL(rangeChanged(QCPRange,QCPRange)), this, SLOT(xAxisRangeChanged(QCPRange,QCPRange)));
    connect(widget->yAxis, SIGNAL(rangeChanged(QCPRange,QCPRange)), this, SLOT(yAxisRangeChanged(QCPRange,QCPRange)));
    connect(widget->yAxis2, SIGNAL(rangeChanged(QCPRange,QCPRange)), this, SLOT(yAxis2RangeChanged(QCPRange,QCPRange)));
    connect(widget, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel(QWheelEvent*)));

    widget->rescaleAxes();

    widget->xAxis->setRange(0, 10, Qt::AlignCenter);
    widget->yAxis->setRange(0, 100, Qt::AlignCenter);
    widget->yAxis2->setRange(0, 40, Qt::AlignCenter);


    widget->replot();

    // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
    //connect(pTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    //pTimer->start(1000); // Interval in msec. 0 means to refresh as fast as possible
}
//--------------------------------------------------------------
//ограничение на масштабирование по оси Х (только положительная часть)
void MainWindow::xAxisRangeChanged(const QCPRange &newRange, const QCPRange &oldRange)
{
    if(newRange.lower < 0){
        ui->widgetPlot->xAxis->setRangeLower( 0 );
        ui->widgetPlot->xAxis->setRangeUpper( oldRange.upper );
    }
    else{
        ui->widgetPlot->xAxis->setRangeUpper( newRange.upper );
    }
}
//--------------------------------------------------------------
//ограничение на масштабирование по оси Y (только положительная часть)
void MainWindow::yAxisRangeChanged(const QCPRange &newRange, const QCPRange &)
{ 
    if(newRange.lower < 0)
        ui->widgetPlot->yAxis->setRangeLower( 0 );

    if(newRange.upper > 100)
        ui->widgetPlot->yAxis->setRangeUpper( 100 );
}
//--------------------------------------------------------------
//ограничение на масштабирование по оси Y2 (только положительная часть)
void MainWindow::yAxis2RangeChanged(const QCPRange &newRange, const QCPRange &)
{
    if(newRange.lower < 0)
        ui->widgetPlot->yAxis2->setRangeLower( 0 );

    if(newRange.upper > 40)
        ui->widgetPlot->yAxis2->setRangeUpper( 40 );
}
//--------------------------------------------------------------
void MainWindow::mouseWheel(QWheelEvent*)
{
    if (ui->widgetPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis)){
            ui->widgetPlot->axisRect()->setRangeZoomAxes(ui->widgetPlot->xAxis,ui->widgetPlot->yAxis);
            ui->widgetPlot->axisRect()->setRangeZoom(ui->widgetPlot->xAxis->orientation());
          }
          else if (ui->widgetPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis)){
            ui->widgetPlot->axisRect()->setRangeZoomAxes(ui->widgetPlot->xAxis,ui->widgetPlot->yAxis);
            ui->widgetPlot->axisRect()->setRangeZoom(ui->widgetPlot->yAxis->orientation());
          }
          else if (ui->widgetPlot->xAxis2->selectedParts().testFlag(QCPAxis::spAxis)){
            ui->widgetPlot->axisRect()->setRangeZoomAxes(ui->widgetPlot->xAxis2,ui->widgetPlot->yAxis2);
            ui->widgetPlot->axisRect()->setRangeZoom(ui->widgetPlot->xAxis2->orientation());
          }
          else if (ui->widgetPlot->yAxis2->selectedParts().testFlag(QCPAxis::spAxis)){
            ui->widgetPlot->axisRect()->setRangeZoomAxes(ui->widgetPlot->xAxis2,ui->widgetPlot->yAxis2);
            ui->widgetPlot->axisRect()->setRangeZoom(ui->widgetPlot->yAxis2->orientation());
          }
          else
            ui->widgetPlot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
}
//--------------------------------------------------------------
//функция для примера, не нужна для работы программы
void MainWindow::realtimeDataSlot()
{
  static QTime time(QTime::currentTime());
  // calculate two new data points:
  double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds
  static double lastPointKey = 0;
  if (key-lastPointKey > 0.002) // at most add point every 2 ms
  {
    // add data to lines:
      //key - значение по оси X
      //(QSin...) - значение по оси Y
    ui->widgetPlot->graph(0)->addData(key, qSin(key)+qrand()/(double)RAND_MAX*1*qSin(key/0.3843));
    ui->widgetPlot->graph(1)->addData(key, qCos(key)+qrand()/(double)RAND_MAX*0.5*qSin(key/0.4364));
    // rescale value (vertical) axis to fit the current data:
    //ui->customPlot->graph(0)->rescaleValueAxis();
    //ui->customPlot->graph(1)->rescaleValueAxis(true);
    lastPointKey = key;
  }

  // make key axis range scroll with the data (at a constant range size of 8):
  ui->widgetPlot->xAxis->setRange(key, 8, Qt::AlignRight);
  ui->widgetPlot->replot();

  // calculate frames per second:
  static double lastFpsKey;
  static int frameCount;
  ++frameCount;
  if (key-lastFpsKey > 2) // average fps over 2 seconds
  {
    /*ui->statusBar->showMessage(
          QString("%1 FPS, Total Data points: %2")
          .arg(frameCount/(key-lastFpsKey), 0, 'f', 0)
          .arg(ui->customPlot->graph(0)->data()->size()+ui->customPlot->graph(1)->data()->size())
          , 0);*/
    lastFpsKey = key;
    frameCount = 0;
  }
}
//--------------------------------------------------------------
