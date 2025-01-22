#include "config.h"
#include "indi_lumix.h"
#include "indidevapi.h"

// declare an auto pointer to LumixCameraDriver
static std::unique_ptr<LumixCameraDriver> lumix_driver(new LumixCameraDriver());

LumixCameraDriver::LumixCameraDriver()
{
    setVersion(INDI_LUMIX_VERSION_MAJOR, INDI_LUMIX_VERSION_MINOR);
}

LumixCameraDriver::~LumixCameraDriver()
{
}

const char * LumixCameraDriver::getDefaultName()
{
    return "Lumix Camera";
}

bool LumixCameraDriver::initProperties()
{
    INDI::CCD::initProperties();

    SaveOnCameraSP[SAVE_ON_CAMERA].fill(
        "SAVE_ON_CAMERA",
        "Save Images on Camera",
        ISS_OFF
    );

    SaveOnCameraSP.fill(
        getDeviceName(),
        "FILE_SAVING",
        "File Saving",
        OPTIONS_TAB,
        IP_WO,
        ISR_ATMOST1,
        5,
        IPS_IDLE
    );

    SaveOnCameraSP.onUpdate([this] {
        switch (SaveOnCameraSP.findOnSwitchIndex()) {
        case SAVE_ON_CAMERA:
            LOG_INFO("Set to save photos on camera.");
            break;
        default:
            LOG_INFO("Set to NOT save photos on camera.");
        }

        SaveOnCameraSP.setState(IPS_IDLE);
        SaveOnCameraSP.apply();
    });

    defineProperty(SaveOnCameraSP);

    CameraInfoTP[MANUFACTURER].fill(
        "MANUFACTURER",
        "Manufacturer",
        "Unknown"
    );

    CameraInfoTP[MODEL].fill(
        "MODEL",
        "Model",
        "Unknown"
    );

    CameraInfoTP[SERIAL].fill(
        "SERIAL",
        "Serial Number",
        "Unknown"
    );

    CameraInfoTP[VERSION].fill(
        "VERSION",
        "Camera Version",
        "Unknown"
    );

    CameraInfoTP.fill(
        getDeviceName(),
        "CAMERA_INFO",
        "Camera Info",
        INFO_TAB,
        IP_RO,
        60,
        IPS_IDLE
    );

    IsoNP.fill(
        getDefaultName(),
        "ISO_VALUE",
        "ISO Value",
        MAIN_CONTROL_TAB,
        IP_RW,
        60,
        IPS_IDLE
    );

    IsoNP.onUpdate([this] {
        if (setIso(IsoNP[0].getValue())) {
            IsoNP.setState(IPS_IDLE);
        } else {
            IsoNP.setState(IPS_ALERT);
        }

        IsoNP.apply();

        int actual_iso;
        if (getIso(&actual_iso)) {
            IsoNP[0].setValue(actual_iso);
        }
    });

    // set which capabilities the camera has
    uint32_t cap = CCD_HAS_SHUTTER;
    SetCCDCapability(cap);

    return true;
}

bool LumixCameraDriver::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected()) {
        setCurrentPollingPeriod(100);
        SetTimer(getCurrentPollingPeriod());
    }

    return true;
}

GPContext* LumixCameraDriver::create_context() {
    GPContext *context = gp_context_new();
    return context;
}

bool LumixCameraDriver::connect_to_lumix_camera() {
    CameraAbilitiesList *abilities_list;
    CameraList *cameras;
    int ret, i;

    camera = nullptr;
    gpContext = create_context();
    ret = gp_camera_new(&camera);
    ret = gp_camera_init(camera, gpContext);
    if (ret != GP_OK) {
        LOG_ERROR("No camera found. Ensure it's connected and powered on.");
        gp_camera_free(camera);
        return false;
    }

    if (!load_camera_widgets()) {
        LOG_ERROR("Failed to load camera widgets!");
        return false;
    }

    if (!load_camera_info()) {
        LOG_ERROR("Failed to load camera info!");
        return false;
    }

    // // Create abilities list
    // ret = gp_abilities_list_new(&abilities_list);
    // if (ret < GP_OK) return false;

    // // Load all supported camera abilities
    // ret = gp_abilities_list_load(abilities_list, gpContext);
    // if (ret < GP_OK) return false;

    // // Detect all cameras
    // ret = gp_abilities_list_detect(abilities_list, NULL, cameras, gpContext);
    // if (ret < GP_OK) return false;

    // // Loop through the detected cameras
    // for (i = 0; i < gp_list_count(cameras); i++) {
    //     const char *name, *value;
    //     CameraAbilities a;
        
    //     // Get camera name and port
    //     gp_list_get_name(cameras, i, &name);
    //     gp_list_get_value(cameras, i, &value);

    //     // Check if this camera is a Panasonic/Lumix
    //     ret = gp_abilities_list_get_abilities(abilities_list, 0, &a);
    //     if (ret == GP_OK) {
    //         if (strstr(a.model, "Panasonic") || strstr(a.model, "Lumix")) {
    //             // Here we found a Panasonic/Lumix camera
    //             GPPortInfo port_info;
    //             gp_port_info_new(&port_info);
                
    //             // Set port information for the camera
    //             gp_port_info_set_path(port_info, value);
    //             gp_camera_set_port_info(camera, port_info);

    //             // Initialize the camera with the specified port
    //             ret = gp_camera_init(camera, gpContext);
    //             if (ret < GP_OK) {
    //                 LOG_ERROR("Camera initialization failed");
    //             } else {
    //                 // Camera is initialized successfully!
    //                 gp_abilities_list_free(abilities_list);
    //                 gp_list_free(cameras);

    //                 // load the widgets
    //                 return load_camera_widgets();
    //             }
    //         }
    //     }
    // }

    // gp_abilities_list_free(abilities_list);
    // gp_list_free(cameras);
    return true;
}

bool LumixCameraDriver::load_camera_widgets() {
    // get config widget
    int ret = gp_camera_get_config(camera, &config, gpContext);
    if (ret != GP_OK) {
        LOG_ERROR("Could not get camera config.");
        return false;
    }

#pragma region ShutterSpeedSetup
    // get shutter speed widget
    ret = gp_widget_get_child_by_name(config, "shutterspeed", &ss);
    if (ret != GP_OK) {
        LOG_ERROR("Could not get camera shutter speed widget.");
        return false;
    }

    // ensure ss widget is a RADIO widget
    CameraWidgetType type;
    ret = gp_widget_get_type(ss, &type);
    if (ret != GP_OK || type != GP_WIDGET_RADIO) {
        LOGF_ERROR("Shutter speed widget is not a RADIO widget, rather it has the value %i.", type);
        return false;
    }

    // if shutter speed is bulb, set it to 1 second (otherwise all choices will be incorrect)
    const char *value;
    ret = gp_widget_get_value(ss, &value);
    if (ret != GP_OK) {
        LOG_ERROR("Could not get current camera shutter speed setting.");
        return false;
    }
    if (strcmp(value, "bulb")) {
        ret = gp_widget_set_value(ss, "1");
        if (ret != GP_OK) {
            LOG_ERROR("Please disable bulb mode or use a shutter speed other than bulb then reconnect the camera.");
            return false;
        }
    }

    // get number of ss widget choices
    int count = gp_widget_count_choices(ss);

    // go through all possible shutter speed values (skip first choice as it is usually bulb mode in a weird format)
    for (int i = 1; i < count; i++) {
        const char *choice;
        ret = gp_widget_get_choice(ss, i, &choice);
        if (ret != GP_OK) {
            LOG_ERROR("Failed to get possible shutter speed value.");
            continue;
        }
        std::string choice_string = std::string(choice);
        float choice_float = 0;
        if (choice_string.rfind("1/", 0) == 0) {
            choice_float = 1.0 / std::stof(choice_string.substr(2));
        } else {
            choice_float = std::stof(choice_string);
        }
        ss_choices.insert({choice_float, choice});
        LOGF_INFO("Possible Shutter Speed Value for choice %i: %f (%s)", i, choice_float, choice);
    }
#pragma endregion ShutterSpeedSetup

#pragma region ISOSetup
    // get iso widget
    ret = gp_widget_get_child_by_name(config, "iso", &iso_w);
    if (ret != GP_OK) {
        LOG_ERROR("Could not get camera iso widget.");
        return false;
    }

    // ensure iso widget is a RADIO widget
    ret = gp_widget_get_type(iso_w, &type);
    if (ret != GP_OK || type != GP_WIDGET_RADIO) {
        LOGF_ERROR("Iso widget is not a RADIO widget, rather it has the value %i.", type);
        return false;
    }

    // get number of iso widget choices
    count = gp_widget_count_choices(iso_w);

    // go through all possible iso values
    for (int i = 0; i < count; i++) {
        const char *choice;
        ret = gp_widget_get_choice(iso_w, i, &choice);
        if (ret != GP_OK) {
            LOG_ERROR("Failed to get possible iso value.");
            continue;
        }
        std::string choice_string = std::string(choice);
        int choice_int = std::stoi(choice_string);
        iso_choices.insert({choice_int, choice});
        LOGF_INFO("Possible iso Value for choice %i: %i", i, choice_int);
    }
#pragma endregion ISOSetup

    return true;
}

bool LumixCameraDriver::load_camera_info() {
    int ret;
    const char *value;
    CameraWidget *child;

    // Get the manufacturer field
    ret = gp_widget_get_child_by_name(config, "manufacturer", &child);
    if (ret == GP_OK) {
        ret = gp_widget_get_value(child, &value);
        if (ret == GP_OK) {
            CameraInfoTP[MANUFACTURER].setText(value);
        } else {
            LOG_ERROR("Failed to get manufacturer name.");
        }
    } else {
        LOG_ERROR("Manufacturer field not found.");
    }

    // Get the model field
    ret = gp_widget_get_child_by_name(config, "cameramodel", &child);
    if (ret == GP_OK) {
        ret = gp_widget_get_value(child, &value);
        if (ret == GP_OK) {
            CameraInfoTP[MODEL].setText(value);
        } else {
            LOG_ERROR("Failed to get camera model.");
        }
    } else {
        LOG_ERROR("Camera model field not found.");
    }

    // Get the serial number field
    ret = gp_widget_get_child_by_name(config, "serialnumber", &child);
    if (ret == GP_OK) {
        ret = gp_widget_get_value(child, &value);
        if (ret == GP_OK) {
            CameraInfoTP[SERIAL].setText(value);
        } else {
            LOG_ERROR("Failed to get serial number.");
        }
    } else {
        LOG_ERROR("Serial number field not found.");
    }

    // Get the version field
    ret = gp_widget_get_child_by_name(config, "deviceversion", &child);
    if (ret == GP_OK) {
        ret = gp_widget_get_value(child, &value);
        if (ret == GP_OK) {
            CameraInfoTP[VERSION].setText(value);
        } else {
            LOG_ERROR("Failed to get device version.");
        }
    } else {
        LOG_ERROR("Device version field not found.");
    }

    defineProperty(CameraInfoTP);

    // Load the ISO param
    ret = gp_widget_get_value(iso_w, &value);
    if (ret == GP_OK) {
        IsoNP[0].fill(
            "ISO",
            "ISO",
            "%i",
            iso_choices.begin()->first,
            std::prev(iso_choices.end())->first,
            1,
            std::stoi(value)
        );
    } else {
        LOG_ERROR("Failed to get camera iso.");
    }

    defineProperty(IsoNP);

    return true;
}

bool LumixCameraDriver::getIso(int *iso) {
    const char *value;
    // Load the ISO param
    int ret = gp_widget_get_value(iso_w, &value);
    if (ret == GP_OK) {
        *iso = std::stoi(value);
        return true;
    } else {
        LOG_ERROR("Failed to get camera iso.");
        return false;
    }
}

bool LumixCameraDriver::Connect()
{
    // Connect to the camera
    try {
        if (!connect_to_lumix_camera()) {
            LOG_ERROR("No Lumix camera found. Ensure it's connected and powered on.");
            return false;
        } else {
            setupParams();
        }
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
    gp_camera_exit(camera, gpContext);
    gp_camera_free(camera);
    gp_context_unref(gpContext);

    LOG_INFO("Disconnected from camera");

    return true;
}

bool LumixCameraDriver::getExposureValue(float duration, const char **value) {
    if (ss_choices.empty()) {
        LOG_ERROR("There are no possible shutter speeds. Try reconnecting your camera.");
        return false;
    }

        // Check if the target is out of bounds
    if (duration < ss_choices.begin()->first || duration > std::prev(ss_choices.end())->first) {
        LOG_ERROR("The set exposure time is outside the capabilities of your camera.");
        return false;
    }

    // Use lower_bound to find the first element not less than target
    auto lower = ss_choices.lower_bound(duration);

    // If lower is the beginning, return its value
    if (lower == ss_choices.begin()) {
        *value = lower->second;
        return true;
    }
    // If lower is the end, return the previous element's value
    if (lower == ss_choices.end()) {
        *value = std::prev(lower)->second;
        return true;
    }

    // Compare the element before `lower` with `lower` to find the closest
    auto prev = std::prev(lower);
    if (std::abs(duration - prev->first) <= std::abs(duration - lower->first)) {
        *value = prev->second;
        return true;
    } else {
        *value = lower->second;
        return true;
    }
}

bool LumixCameraDriver::getIsoChoiceValue(int iso, const char **value) {
    if (iso_choices.empty()) {
        LOG_ERROR("There are no possible iso values. Try reconnecting your camera.");
        return false;
    }

    // Check if the target is out of bounds
    if (iso < iso_choices.begin()->first || iso > std::prev(iso_choices.end())->first) {
        LOG_ERROR("The set iso value is outside the capabilities of your camera.");
        return false;
    }

    // Use lower_bound to find the first element not less than target
    auto lower = iso_choices.lower_bound(iso);

    // If lower is the beginning, return its value
    if (lower == iso_choices.begin()) {
        *value = lower->second;
        return true;
    }
    // If lower is the end, return the previous element's value
    if (lower == iso_choices.end()) {
        *value = std::prev(lower)->second;
        return true;
    }

    // Compare the element before `lower` with `lower` to find the closest
    auto prev = std::prev(lower);
    if (std::abs(iso - prev->first) <= std::abs(iso - lower->first)) {
        *value = prev->second;
        return true;
    } else {
        *value = lower->second;
        return true;
    }
}

bool LumixCameraDriver::setupParams()
{
    float x_pixel_size, y_pixel_size;
    int bit_depth = 16; // valid values are 8, 16, 32
    int x_1, y_1, x_2, y_2;
    int channels = 3;

    // TODO: Actually get the pixel size from the camera
    x_pixel_size = 5.95;
    y_pixel_size = 5.95;

    // TODO: Actually get the image size from the camera
    x_1 = y_1 = 0;
    x_2 = 6008;
    y_2 = 4008;

    // Set the pixel size
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Set the channels
    PrimaryCCD.setNAxis(channels);

    // TODO: Now we usually do the following in the hardware
    // Set Frame to LIGHT or NORMAL
    // Set Binning to 1x1
    /* Default frame type is NORMAL */

    // Calculate the required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * ((PrimaryCCD.getBPP() * PrimaryCCD.getNAxis()) / 8); // this is the pixel count
    nbuf += 512; // add some extra buffer
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool LumixCameraDriver::setShutterSpeed(float duration) {
    const char *value;
    if (!getExposureValue(duration, &value)) {
        return false;
    }

    LOGF_INFO("Setting shutter speed to %s", value);
    int ret = gp_widget_set_value(ss, value);
    if (ret != GP_OK) {
        LOGF_ERROR("Failed to set shutter speed to %s.", value);
        return false;
    }

    // apply the changes
    ret = gp_camera_set_config(camera, config, gpContext);
    if (ret != GP_OK) {
        LOG_ERROR("Failed to apply shutter speed to camera.");
        return false;
    }

    return true;
}

bool LumixCameraDriver::setIso(int iso) {
    const char *value;
    if (!getIsoChoiceValue(iso, &value)) {
        return false;
    }

    LOGF_INFO("Setting iso to %s", value);
    int ret = gp_widget_set_value(iso_w, value);
    if (ret != GP_OK) {
        LOGF_ERROR("Failed to set iso to %s.", value);
        return false;
    }

    // apply the changes
    ret = gp_camera_set_config(camera, config, gpContext);
    if (ret != GP_OK) {
        LOG_ERROR("Failed to apply iso to camera.");
        return false;
    }

    return true;
}

bool LumixCameraDriver::StartExposure(float duration)
{
    // Set the exposure request
    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    // start the takePhoto process in a separate thread
    std::thread([this, duration] {
        // int ret = gp_widget_set_value(ss, "bulb");
        // if (ret < GP_OK) {
        //     LOG_ERROR("Could not set camera mode to bulb.");
        //     return;
        // }

        // ret = gp_camera_set_config(camera, config, gpContext);
        // if (ret < GP_OK) {
        //     LOG_ERROR("Could not update camera config.");
        //     return;
        // }
        if (!setShutterSpeed(duration)) {
            LOG_ERROR("Could not set proper shutter speed!");
            return;
        }

        isExposing = true;

        // only open the shutter because of bulb mode
        int ret = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &filePath, gpContext);;
        if (ret < GP_OK) {
            LOG_ERROR((std::string("Error starting exposure: ") + std::string(gp_result_as_string(ret))).c_str());
        }
        LOG_INFO("Capture finished successfully!");

        //sleep(duration);

        // close the shutter
        // ret = gp_camera_trigger_capture(camera, gpContext);
        // if (ret < GP_OK) {
        //     LOG_ERROR((std::string("Error stopping exposure: ") + std::string(gp_result_as_string(ret))).c_str());
        // }

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
    nbuf = (bin_width * bin_height * PrimaryCCD.getNAxis() * PrimaryCCD.getBPP() / 8); // this is the pixel count
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
    int channels   = PrimaryCCD.getNAxis();

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

    LOG_INFO("Starting Copy...");

    // download the photo
    CameraFile *file = nullptr;
    gp_file_new(&file);

    int ret = gp_camera_file_get(camera, filePath.folder, filePath.name, GP_FILE_TYPE_RAW, file, gpContext);
    if (ret < GP_OK) {
        LOG_ERROR("Failed to download image from camera...");
        return -1;
    }

    const char *data;
    unsigned long int size;
    gp_file_get_data_and_size(file, &data, &size);

    // TODO: add support for non raw images
    std::string filename = filePath.name;
    if (filename.substr(filename.find_last_of(".") + 1) != "RW2") {
        LOG_ERROR("This driver currently does not support non-RW2 files. Please select RAW picture quality on your camera.");
        return -1;
    }

    // start decoding Raw image
    LibRaw raw_processor;
    int raw_ret = raw_processor.open_buffer(data, size);
    if (raw_ret != LIBRAW_SUCCESS) {
        LOG_ERROR("Could not load camera RAW file into LibRaw.");
        return -1;
    }

    // Unpack the RAW data
    raw_ret = raw_processor.unpack();
    if (raw_ret != LIBRAW_SUCCESS) {
        LOG_ERROR("Unable to unpack the RAW data.");
        return -11;
    }

    // Set processing parameters
    libraw_output_params_t* params = raw_processor.output_params_ptr();
    // always upsampled to 16 bits
    params->output_bps = 16; // Use 16 bits per channel

    // Process image to include color and debayer step
    if (raw_processor.dcraw_process() != LIBRAW_SUCCESS) {
        LOG_ERROR("Unable to process the RAW data.");
        return -1;
    }

    // Get the processed image data
    libraw_processed_image_t* raw_image = raw_processor.dcraw_make_mem_image();
    if (!raw_image) {
        LOG_ERROR("Unable to allocate memory for the processed image.");
        return -1;
    }

    LOGF_INFO("Raw Image size: %i, Expected Size: %i", raw_image->data_size, (width * height * channels * (bpp / 8.0)));
    LOGF_INFO("Width: %i, Height: %i, Channels: %i, BPP: %i", width, height, channels, bpp);
    if (raw_image->data_size != (width * height * channels * (bpp / 8.0))) {
        LOG_ERROR("Error: Image size does not match expected size");

        return -1;
    }

    // the numbers of bytes per pixel (think of as Pixel Bytes)
    int pb = bpp/8;

    // Copy rgbrgb... format from raw_image to rrr...ggg...bbb... in the Indi pixel buffer
    for (int i = 0; i < width*height; i++) {
        for (int c = 0; c < channels; c++) {
            int srcIndex = i * channels * pb + c * pb;
            int destIndex = width * height * c * pb + i * pb;

            for (int b = 0; b < pb; b++) {
                image[destIndex+b] = raw_image->data[srcIndex+b];
            }
        }
    }

    // clear opened buffer
    LibRaw::dcraw_clear_mem(raw_image);

    // delete image off of camera if set to not save on camera
    if (SaveOnCameraSP.findOnSwitchIndex() != SAVE_ON_CAMERA) {
        ret = gp_camera_file_delete(camera, filePath.folder, filePath.name, gpContext);
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
    // switch (TemperatureNP.s)
    // {
    //     case IPS_IDLE:
    //     case IPS_OK:
    //         /**********************************************************
    //         *
    //         *
    //         *
    //         *  IMPORRANT: Put here your CCD Get temperature call here
    //         *  If there is an error, report it back to client
    //         *  e.g.
    //         *  LOG_INFO( "Error, unable to get temp due to ...");
    //         *  return false;
    //         *
    //         *
    //         **********************************************************/
    //         break;

    //     case IPS_BUSY:
    //         /**********************************************************
    //         *
    //         *
    //         *
    //         *  IMPORRANT: Put here your CCD Get temperature call here
    //         *  If there is an error, report it back to client
    //         *  e.g.
    //         *  LOG_INFO( "Error, unable to get temp due to ...");
    //         *  return false;
    //         *
    //         *
    //         **********************************************************/
    //         //TemperatureN[0].value = TemperatureRequest;

    //         // If we're within threshold, let's make it BUSY ---> OK
    //         //if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
    //         //    TemperatureNP.s = IPS_OK;

    //         //IDSetNumber(&TemperatureNP, nullptr);
    //         break;

    //     case IPS_ALERT:
    //         break;
    // }

    SetTimer(getCurrentPollingPeriod());
    return;
}

void LumixCameraDriver::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) {
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    std::string manufacturer = std::string(CameraInfoTP[MANUFACTURER].getText());
    std::string model = std::string(CameraInfoTP[MODEL].getText());
    if (manufacturer.compare("Unknown") != 0 && model.compare("Unknown") != 0) {
        // first remove the old INSTRUME record
        auto record = std::find_if(fitsKeywords.begin(), fitsKeywords.end(), [&fitsKeywords](const INDI::FITSRecord& x) { return strcmp(x.key().c_str(), "INSTRUME"); });
        if (record != fitsKeywords.end()) {
            fitsKeywords.erase(record);
        }

        // now add the custom INSTRUME record based on the camera info
        fitsKeywords.push_back({"INSTRUME", (manufacturer + std::string(" ") + model).c_str(), "Camera model"});
    }

    fitsKeywords.push_back({"INPUTFMT", "RW2", "Format of file from which image was read"});

    int actual_iso;
    if (getIso(&actual_iso)) {
        fitsKeywords.push_back({"ISOSPEED", std::to_string(actual_iso).c_str(), "ISO camera setting"});
    }
}
