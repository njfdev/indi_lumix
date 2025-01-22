#pragma once

#include <libindi/indiccd.h>
#include <indielapsedtimer.h>
#include <gphoto2/gphoto2-camera.h>
#include <libraw/libraw.h>
#include <unistd.h>
#include <map>

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

    // FITS Header Values to add/adjust: INSTRUME, INPUTFMT, ISOSPEED, 
    void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

    void TimerHit() override;
    //virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    //virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

private:
    // variables for dealing with gphoto2
    Camera *camera;
    GPContext *gpContext;
    // stores the latest file path details of the most recent photo
    CameraFilePath filePath;
    // stores camera widgets (basically settings)
    CameraWidget *config;
    CameraWidget *iso_w;
    CameraWidget *ss; // shutter speed
    // camera setting possible choices
    std::map<float, const char*> ss_choices;
    std::map<int, const char*> iso_choices;

    // functions for dealing with gphoto2
    GPContext* create_context();
    bool connect_to_lumix_camera();
    bool load_camera_widgets();
    bool load_camera_info();
    void error_func(GPContext *context, const char *str, void *data);
    void status_func(GPContext *context, const char *str, void *data);

    // define Indi properties
    INDI::PropertyNumber IsoNP {1};
    INDI::PropertyText CameraInfoTP {4};
    enum {
        MANUFACTURER,
        MODEL,
        SERIAL,
        VERSION
    };
    INDI::PropertySwitch SaveOnCameraSP {1};
    enum {
        SAVE_ON_CAMERA
    };

    INDI::ElapsedTimer m_ElapsedTimer;
    double ExposureRequest;
    bool isExposing = false;

    int downloadImage();
    bool setupParams();
    bool getExposureValue(float duration, const char **value);
    bool setShutterSpeed(float duration);
    bool getIsoChoiceValue(int iso, const char **value);
    bool setIso(int iso);
    bool getIso(int *iso);
};