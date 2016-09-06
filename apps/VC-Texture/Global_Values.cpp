//----------------------------------------------------------------------------------------------------------------------------------------
// Global_Values.cpp file for Global_Values Class
// Purpose: Implements Global_Values Class
// Developer: Michael Royal - mgro224@g.uky.edu
// October 12, 2015 - Spring Semester 2016
// Last Updated 10/23/2015 by: Michael Royal

// Copy Right ©2015 (Brent Seales: Volume Cartography Research) - University of Kentucky Center for Visualization and Virtualization
//----------------------------------------------------------------------------------------------------------------------------------------

#include "Global_Values.h"

Global_Values::Global_Values(QRect rec) : _active(false)
{
    // GETS THE MONITOR'S SCREEN Length & Width
    height = rec.height();
    width = rec.width();

}// End of Default Constructor()

int Global_Values::getHeight() const
{
    return height;
}

int Global_Values::getWidth() const
{
    return width;
}

VolumePkg * Global_Values::getVolPkg() const
{
    return vpkg;
}

void Global_Values::setPath(QString newPath)
{
    path = newPath;
}

void Global_Values::createVolumePackage()
{
    vpkg = new VolumePkg(path.toStdString());// Creates a Volume Package
    VPKG_Instantiated = true;
}

void Global_Values::getMySegmentations()
{
    segmentations = vpkg->getSegmentations();
}

std::vector<std::string> Global_Values::getSegmentations() const
{
    return segmentations;
}

void Global_Values::setQPixMapImage(QImage image)
{
    pix = QPixmap::fromImage(image);
}

QPixmap Global_Values::getQPixMapImage() const
{
    return pix;
}

bool Global_Values::isVPKG_Intantiated()
{
    return VPKG_Instantiated;
}

void Global_Values::setWindow(QMainWindow *window)
{
    _window = window;
}

QMainWindow *Global_Values::getWindow() const
{
    return _window;
}

void Global_Values::setRendering(volcart::Rendering rendering)
{
    _rendering = rendering;
}

void Global_Values::clearRendering()
{
    volcart::Rendering *empty = new volcart::Rendering;
    _rendering = *empty;
}

volcart::Rendering Global_Values::getRendering() const
{
    return _rendering;
}

void Global_Values::setRadius(double radius)
{
    _radius = radius;
}

double Global_Values::getRadius() const
{
    return _radius;
}

void Global_Values::setTextureMethod(int textureMethod)
{
    _textureMethod = textureMethod;
}

int Global_Values::getTextureMethod() const
{
    return _textureMethod;
}

void Global_Values::setSampleDirection(int sampleDirection)
{
    _sampleDirection = sampleDirection;
}

int Global_Values::getSampleDirection() const
{
    return _sampleDirection;
}

void Global_Values::setProcessing(bool active)
{
    _active = active;
}

bool Global_Values::getProcessing() const
{
    return _active;
}

void Global_Values::setForcedClose(bool forcedClose)
{
    _forcedClose = forcedClose;
}

bool Global_Values::getForcedClose()
{
    return _forcedClose;
}

void Global_Values::setStatus(int status)
{
    _status = status;
}

int Global_Values::getStatus() const
{
    return _status;
}

void Global_Values::setFileMenu(QMenu *fileMenu)
{
    _fileMenu = fileMenu;
}

void Global_Values::enableMenus(bool value)
{
    _fileMenu->setEnabled(value);
}
