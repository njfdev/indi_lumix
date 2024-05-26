#include "config.h"
#include "indi_lumix.h"
#include "indidevapi.h"

// declare an auto pointer to LumixS5IIX
static std::unique_ptr<LumixCameraDriver> lumix_s5iix_driver(new LumixCameraDriver());

LumixCameraDriver::LumixCameraDriver()
{
    setVersion(INDI_LUMIX_VERSION_MAJOR, INDI_LUMIX_VERSION_MINOR);
    setDeviceName("Lumix S5IIX");
}

LumixCameraDriver::~LumixCameraDriver()
{
}

const char * LumixCameraDriver::getDefaultName()
{
    return "Lumix S5IIX";
}

bool LumixCameraDriver::initProperties()
{
    INDI::CCD::initProperties();

    CameraIPAddressTP[0].fill("CAMERA_IP", "Camera IP Address", "");
    CameraIPAddressTP.fill(getDeviceName(), "CAMERA_IP", "Camera IP Address", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    CameraIPAddressTP.onUpdate([this]
    {
        CameraIPAddressTP.setState(IPS_OK);
        CameraIPAddressTP.apply();
    });

    // set which capabilities the camera has
    uint32_t cap = CCD_HAS_SHUTTER | CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME;
    SetCCDCapability(cap);

    // Add configuration for debug
    addDebugControl();

    return true;
}

bool LumixCameraDriver::updateProperties()
{
    defineProperty(CameraIPAddressTP);

    INDI::CCD::updateProperties();

    if (isConnected()) {
        setupParams();

        // if connected, disable camera IP address write
        CameraIPAddressTP.setPermission(IP_RO);

        SetTimer(getCurrentPollingPeriod());
    } else {
        // if not connected, enable camera IP address write
        CameraIPAddressTP.setPermission(IP_RW);
    }

    return true;
}

bool LumixCameraDriver::Connect()
{
    // Connect to the camera
    try {
        camera = std::make_unique<Lumix::Camera>(CameraIPAddressTP[0].getText(), "INDI Lumix Driver");
    } catch (const std::exception& e) {
        LOG_ERROR("Error connecting to camera");

        return false;
    }

    LOG_INFO("Connected to camera");

    return true;
}

bool LumixCameraDriver::Disconnect()
{
    // Disconnect from the camera
    camera.reset();

    LOG_INFO("Disconnected from camera");

    return true;
}

bool LumixCameraDriver::setupParams()
{
    float x_pixel_size, y_pixel_size;
    int bit_depth = 8; // valid values are 8, 16, 32
    int x_1, y_1, x_2, y_2;

    // TODO: Actually get the pixel size from the camera
    x_pixel_size = 5.95;
    y_pixel_size = 5.95;

    // TODO: Actually get the image size from the camera
    x_1 = y_1 = 0;
    x_2 = 6000;
    y_2 = 4000;

    // Set the pixel size
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // TODO: Now we usually do the following in the hardware
    // Set Frame to LIGHT or NORMAL
    // Set Binning to 1x1
    /* Default frame type is NORMAL */

    // Calculate the required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * (PrimaryCCD.getBPP() / 8); // this is the pixel count
    nbuf += 512; // add some extra buffer
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool LumixCameraDriver::StartExposure(float duration)
{
    // Set the exposure request
    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    // start the takePhoto process in a separate thread
    std::thread([this, duration] {
        isExposing = true;
        camera->TakePhoto(duration);
        isExposing = false;
    }).detach();

    m_ElapsedTimer.start();
    InExposure = true;

    return true;
}

bool LumixCameraDriver::AbortExposure()
{
    // TODO: actually abort the exposure
    InExposure = false;

    return true;
}

bool LumixCameraDriver::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) {
    INDI::CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

    if (fType == imageFrameType) {
        return true;
    }

    // TODO: Actually implement this
    switch (imageFrameType)
    {
        case INDI::CCDChip::BIAS_FRAME:
        case INDI::CCDChip::DARK_FRAME:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Frame type here
            *  BIAS and DARK are taken with shutter closed, so _usually_
            *  most CCD this is a call to let the CCD know next exposure shutter
            *  must be closed. Customize as appropiate for the hardware
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to set frame type to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;

        case INDI::CCDChip::LIGHT_FRAME:
        case INDI::CCDChip::FLAT_FRAME:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Frame type here
            *  LIGHT and FLAT are taken with shutter open, so _usually_
            *  most CCD this is a call to let the CCD know next exposure shutter
            *  must be open. Customize as appropiate for the hardware
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to set frame type to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;
    }

    PrimaryCCD.setFrameType(fType);

    return true;
}

bool LumixCameraDriver::UpdateCCDFrame(int x, int y, int w, int h) {
    // add the x and y offsets
    long x_1 = x;
    long y_1 = y;

    long bin_width = x_1 + (w / PrimaryCCD.getBinX());
    long bin_height = y_1 + (h / PrimaryCCD.getBinY());

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX()) {
        LOG_INFO("Error: X offset + width is greater than the CCD width");

        return false;
    } else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY()) {
        LOG_INFO("Error: Y offset + height is greater than the CCD height");

        return false;
    }

    /**********************************************************
    *
    *
    *
    *  TODO: Put here your CCD Frame dimension call
    *  The values calculated above are BINNED width and height
    *  which is what most CCD APIs require, but in case your
    *  CCD API implementation is different, don't forget to change
    *  the above calculations.
    *  If there is an error, report it back to client
    *  e.g.
    *  LOG_INFO( "Error, unable to set frame to ...");
    *  return false;
    *
    *
    **********************************************************/

    // set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, bin_width, bin_height);

    int nbuf;
    nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8); // this is the pixel count
    nbuf += 512; // add some extra buffer
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool LumixCameraDriver::UpdateCCDBin(int binx, int biny)
{
    /**********************************************************
    *
    *
    *
    *  TODO: Put here your CCD Binning call
    *  If there is an error, report it back to client
    *  e.g.
    *  LOG_INFO( "Error, unable to set binning to ...");
    *  return false;
    *
    *
    **********************************************************/

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

int LumixCameraDriver::downloadImage()
{
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    int bpp        = PrimaryCCD.getBPP();

    /**********************************************************
     *
     *
     *  TODO: Put here your CCD Get Image routine here
     *  use the image, width, and height variables above
     *  If there is an error, report it back to client
     *
     *
     **********************************************************/
    // For now, just fill the image with random data
    /*for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            image[i * width + j] = rand() % 255;*/

    Lumix::ImageData imageData;
    if (!camera->DownloadLatestPhoto(imageData)) {
        LOG_ERROR("Error downloading image");

        return -1;
    }

    if (!camera->GetRawPixelData(imageData)) {
        LOG_ERROR("Error getting raw pixel data");

        return -1;
    }

    /* TODO: fix this because I was having issues
    if (imageData.rawPixelData.size() != (width * height * (PrimaryCCD.getBPP() / 8))) {
        LOG_ERROR("Error: Image size does not match expected size");

        return -1;
    }
    */

    LOG_INFO("Starting Copy...");
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // set red, green values
            uint8_t red = imageData.pixelBuffer[(i * width)*3 + j*3];
            uint8_t green = imageData.pixelBuffer[(i * width)*3 + j*3 + 1];
            uint8_t blue = imageData.pixelBuffer[(i * width)*3 + j*3 + 2];

            // average the red, green, and blue values
            uint8_t luminosity = (red + green + blue) / 3;

            // set the pixel value
            image[i * width + j] = luminosity;
        }
    }

    LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
/// TimerHit is the main loop of the driver where it gets called every 1 second
/// by default. Here you perform checks on any ongoing operations and perhaps query some
/// status updates like temperature.
///////////////////////////////////////////////////////////////////////////////////////
void LumixCameraDriver::TimerHit()
{
    if (isConnected() == false)
        return;

    // Are we in exposure? Let's check if we're done!
    if (InExposure)
    {
        // Seconds elapsed
        double timeLeft = ExposureRequest - m_ElapsedTimer.elapsed() / 1000.0;
        if (timeLeft <= 0 && !isExposing)
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");

            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            // Download Image
            downloadImage();
        }
        else
            // set the remaining exposure time (make sure it's not negative)
            PrimaryCCD.setExposureLeft(std::max(0.0, timeLeft));
    }

    // TODO: use this syntax to handle ISO, shutter speed, and aperture
    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Get temperature call here
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to get temp due to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;

        case IPS_BUSY:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Get temperature call here
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to get temp due to ...");
            *  return false;
            *
            *
            **********************************************************/
            //TemperatureN[0].value = TemperatureRequest;

            // If we're within threshold, let's make it BUSY ---> OK
            //if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
            //    TemperatureNP.s = IPS_OK;

            //IDSetNumber(&TemperatureNP, nullptr);
            break;

        case IPS_ALERT:
            break;
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}