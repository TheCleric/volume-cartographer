//---------------------------------------------------------------------------------------------------------------------------------------------
// Global_Values.h file for Global_Values Class
// Purpose: Used to pass values through the Program, Data that needs to be
// shared between several Objects should be declared in globals
// Developer: Michael Royal - mgro224@g.uky.edu
// October 12, 2015 - Spring Semester 2016
// Last Updated 10/23/2015 by: Michael Royal

// Copy Right ©2015 (Brent Seales: Volume Cartography Research) - University of
// Kentucky Center for Visualization and Virtualization
//---------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include <QApplication>
#include <QImage>
#include <QMainWindow>
#include <QMenuBar>
#include <QPixmap>
#include <QRect>
#include "common/types/Rendering.h"
#include "common/vc_defines.h"
#include "volumepkg/volumepkg.h"
#include "volumepkg/volumepkg.h"

// Volpkg version required byt this app
static constexpr int VOLPKG_SUPPORTED_VERSION = 3;

class Global_Values
{

public:
    Global_Values(QRect rec);

    int getHeight();
    int getWidth();

    void createVolumePackage();
    VolumePkg* getVolPkg();

    void setPath(QString newPath);

    void getMySegmentations();
    std::vector<std::string> getSegmentations();

    void setQPixMapImage(QImage image);
    QPixmap getQPixMapImage();

    bool isVPKG_Intantiated();

    void setWindow(QMainWindow* window);
    QMainWindow* getWindow();

    void setRendering(volcart::Rendering rendering);
    void clearRendering();
    volcart::Rendering getRendering();

    void setRadius(double radius);
    double getRadius();

    void setTextureMethod(int textureMethod);
    int getTextureMethod();

    void setSampleDirection(int sampleDirection);
    int getSampleDirection();

    void setProcessing(bool active);
    bool getProcessing();

    void setForcedClose(bool forcedClose);
    bool getForcedClose();

    void setStatus(int status);
    int getStatus();

    void setFileMenu(QMenu* fileMenu);

    void enableMenus(bool value);

private:
    bool VPKG_Instantiated = false;
    int height;
    int width;
    QString path;
    VolumePkg* vpkg;
    std::vector<std::string> segmentations;
    QPixmap pix;
    QMainWindow* _window;
    volcart::Rendering _rendering;
    double _radius;
    int _textureMethod;
    int _sampleDirection;
    int _status;

    QMenu* _fileMenu;

    bool _active;
    bool _forcedClose;
};
