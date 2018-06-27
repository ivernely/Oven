#include "mainwindow.h"
#include <QApplication>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents, true);

    QSystemSemaphore semaphore("Oven-App-ExpCent-KD-1000", 1);
    semaphore.acquire();

#ifndef Q_OS_WIN32
    // linux/unix
    QSharedMemory nix_fix_shared_memory("Oven-App-ExpCent-KD-1000_2");
    if(nix_fix_shared_memory.attach()){
        nix_fix_shared_memory.detach();
    }
#endif

    QSharedMemory sharedMemory("Oven-App-ExpCent-KD-1000_2");
    bool is_running;

    if (sharedMemory.attach())
        is_running = true;
    else{
        sharedMemory.create(1);
        is_running = false;
    }

    semaphore.release();

    if(is_running){
        QMessageBox msg;
        msg.addButton(QObject::trUtf8("OK"), QMessageBox::AcceptRole);
        msg.setText(QObject::trUtf8("Приложение для управления испытательной камерой уже запущено.\n"
                                    "Вы можете запустить только один экземпляр данного приложения."));
        msg.setWindowTitle(QObject::trUtf8("Ошибка"));
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowModality(Qt::WindowModal);
        msg.setWindowFlags(Qt::BypassWindowManagerHint);
        msg.exec();
        return 1;
        }


    MainWindow w;

#if TARGET_OS==RASPBIAN_OS
        //полноэкранный запуск
        //w.setWindowFlags( Qt::Window | Qt::WindowStaysOnTopHint);
        //w.setWindowState( Qt::WindowFullScreen | Qt::WindowActive);
        w.showFullScreen();
        w.raise();
#else
        w.show();
#endif

    return a.exec();
}
