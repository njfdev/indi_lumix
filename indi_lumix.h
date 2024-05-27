#pragma once

#include <liblumix.h>

#include <libindi/indiccd.h>
#include <indielapsedtimer.h>

class LumixCameraDriver : public INDI::CCD
{
public:
    LumixCameraDriver();
    virtual ~LumixCameraDriver();

    virtual const char *getDefaultName() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool AbortExposure() override;

protected:
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool saveConfigItems(FILE *fp) override;

    void TimerHit() override;
    //virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    //virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

private:
    std::unique_ptr<Lumix::Camera> camera;

    // define Indi properties for iso, shutter speed, and aperture
    /*INDI::PropertyNumber IsoNP {1};
    INDI::PropertyNumber ShutterSpeedNP {1};
    INDI::PropertyNumber ApertureNP {1};*/

    INDI::PropertyText CameraIPAddressTP {1};

    INDI::ElapsedTimer m_ElapsedTimer;
    double ExposureRequest;
    bool isExposing = false;

    int downloadImage();
    bool setupParams();
};