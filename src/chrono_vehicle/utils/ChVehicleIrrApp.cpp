// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Irrlicht-based visualization wrapper for vehicles.  This class is a wrapper
// around a ChIrrApp object and provides the following functionality:
//   - rendering of the entire Irrlicht scene
//   - implements a custom chase-camera (which can be controlled with keyboard)
//   - optional rendering of links, springs, stats, etc.
//
// =============================================================================

#include <algorithm>

#include "chrono_vehicle/utils/ChVehicleIrrApp.h"

#include "chrono_vehicle/driveline/ChShaftsDriveline2WD.h"
#include "chrono_vehicle/driveline/ChShaftsDriveline4WD.h"
#include "chrono_vehicle/powertrain/ChShaftsPowertrain.h"

using namespace irr;

namespace chrono {

// -----------------------------------------------------------------------------
// Implementation of the custom Irrlicht event receiver for camera control
// -----------------------------------------------------------------------------
bool ChCameraEventReceiver::OnEvent(const SEvent& event) {
    // Only interpret keyboard inputs.
    if (event.EventType != EET_KEY_INPUT_EVENT)
        return false;

    if (event.KeyInput.PressedDown) {
        switch (event.KeyInput.Key) {
            case KEY_DOWN:
                m_app->m_camera.Zoom(1);
                return true;
            case KEY_UP:
                m_app->m_camera.Zoom(-1);
                return true;
            case KEY_LEFT:
                m_app->m_camera.Turn(1);
                return true;
            case KEY_RIGHT:
                m_app->m_camera.Turn(-1);
                return true;
        }
    } else {
        switch (event.KeyInput.Key) {
            case KEY_KEY_1:
                m_app->m_camera.SetState(utils::ChChaseCamera::Chase);
                return true;
            case KEY_KEY_2:
                m_app->m_camera.SetState(utils::ChChaseCamera::Follow);
                return true;
            case KEY_KEY_3:
                m_app->m_camera.SetState(utils::ChChaseCamera::Track);
                return true;
            case KEY_KEY_4:
                m_app->m_camera.SetState(utils::ChChaseCamera::Inside);
                return true;
            case KEY_KEY_V:
                m_app->m_car.LogConstraintViolations();
                return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// Construct a vehicle Irrlicht application.
// -----------------------------------------------------------------------------
ChVehicleIrrApp::ChVehicleIrrApp(ChVehicle& car,
                                 ChPowertrain& powertrain,
                                 const wchar_t* title,
                                 irr::core::dimension2d<irr::u32> dims)
    : ChIrrApp(car.GetSystem(), title, dims),
      m_car(car),
      m_powertrain(powertrain),
      m_camera(car.GetChassis()),
      m_stepsize(1e-3),
      m_HUD_x(740),
      m_HUD_y(20),
      m_renderGrid(true),
      m_renderLinks(true),
      m_renderSprings(true),
      m_renderStats(true),
      m_gridHeight(0.02) {
    // Initialize the chase camera with default values.
    m_camera.Initialize(ChVector<>(0, 0, 1), car.GetLocalDriverCoordsys(), 6.0, 0.5);
    ChVector<> cam_pos = m_camera.GetCameraPos();
    ChVector<> cam_target = m_camera.GetTargetPos();

    // Create the event receiver for controlling the chase camera.
    m_camera_control = new ChCameraEventReceiver(this);
    SetUserEventReceiver(m_camera_control);

    // Create and initialize the Irrlicht camera
    scene::ICameraSceneNode* camera = GetSceneManager()->addCameraSceneNode(
        GetSceneManager()->getRootSceneNode(), core::vector3df(0, 0, 0), core::vector3df(0, 0, 0));

    camera->setUpVector(core::vector3df(0, 0, 1));
    camera->setPosition(core::vector3df((f32)cam_pos.x, (f32)cam_pos.y, (f32)cam_pos.z));
    camera->setTarget(core::vector3df((f32)cam_target.x, (f32)cam_target.y, (f32)cam_target.z));

#ifdef CHRONO_IRRKLANG
    m_sound_engine = 0;
    m_car_sound = 0;
#endif
}

ChVehicleIrrApp::~ChVehicleIrrApp() {
    delete m_camera_control;
}

// -----------------------------------------------------------------------------
// Turn on/off Irrklang sound generation.
// Note that this has an effect only if Irrklang support was enabled at
// configuration.
// -----------------------------------------------------------------------------
void ChVehicleIrrApp::EnableSound(bool sound) {
#ifdef CHRONO_IRRKLANG
    if (sound) {
        // Start the sound engine with default parameters
        m_sound_engine = irrklang::createIrrKlangDevice();

        // To play a sound, call play2D(). The second parameter tells the engine to
        // play it looped.
        if (m_sound_engine) {
            m_car_sound = m_sound_engine->play2D(GetChronoDataFile("carsound.ogg").c_str(), true, false, true);
            m_car_sound->setIsPaused(true);
        } else
            GetLog() << "Cannot start sound engine Irrklang \n";
    } else {
        m_sound_engine = 0;
        m_car_sound = 0;
    }
#endif
}

// -----------------------------------------------------------------------------
// Create a skybox that has Z pointing up.
// Note that the default ChIrrApp::AddTypicalSky() uses Y up.
// -----------------------------------------------------------------------------
void ChVehicleIrrApp::SetSkyBox() {
    std::string mtexturedir = GetChronoDataFile("skybox/");
    std::string str_lf = mtexturedir + "sky_lf.jpg";
    std::string str_up = mtexturedir + "sky_up.jpg";
    std::string str_dn = mtexturedir + "sky_dn.jpg";
    irr::video::ITexture* map_skybox_side = GetVideoDriver()->getTexture(str_lf.c_str());
    irr::scene::ISceneNode* mbox = GetSceneManager()->addSkyBoxSceneNode(
        GetVideoDriver()->getTexture(str_up.c_str()), GetVideoDriver()->getTexture(str_dn.c_str()), map_skybox_side,
        map_skybox_side, map_skybox_side, map_skybox_side);
    mbox->setRotation(irr::core::vector3df(90, 0, 0));
}

// -----------------------------------------------------------------------------
// Set parameters for the underlying chase camera.
// -----------------------------------------------------------------------------
void ChVehicleIrrApp::SetChaseCamera(const ChVector<>& ptOnChassis, double chaseDist, double chaseHeight) {
    m_camera.Initialize(ptOnChassis, m_car.GetLocalDriverCoordsys(), chaseDist, chaseHeight);
    ChVector<> cam_pos = m_camera.GetCameraPos();
    ChVector<> cam_target = m_camera.GetTargetPos();
}

// -----------------------------------------------------------------------------
// Advance the dynamics of the chase camera.
// The integration of the underlying ODEs is performed using as many steps as
// needed to advance by the specified duration.
// -----------------------------------------------------------------------------
void ChVehicleIrrApp::Advance(double step) {
    // Update the ChChaseCamera: take as many integration steps as needed to
    // exactly reach the value 'step'
    double t = 0;
    while (t < step) {
        double h = std::min<>(m_stepsize, step - t);
        m_camera.Update(step);
        t += h;
    }

    // Update the Irrlicht camera
    ChVector<> cam_pos = m_camera.GetCameraPos();
    ChVector<> cam_target = m_camera.GetTargetPos();

    scene::ICameraSceneNode* camera = GetSceneManager()->getActiveCamera();

    camera->setPosition(core::vector3df((f32)cam_pos.x, (f32)cam_pos.y, (f32)cam_pos.z));
    camera->setTarget(core::vector3df((f32)cam_target.x, (f32)cam_target.y, (f32)cam_target.z));

#ifdef CHRONO_IRRKLANG
    static int stepsbetweensound = 0;

    // Update sound pitch
    if (m_car_sound) {
        stepsbetweensound++;
        double engine_rpm = m_powertrain.GetMotorSpeed() * 60 / chrono::CH_C_2PI;
        double soundspeed = engine_rpm / (8000.);  // denominator: to guess
        if (soundspeed < 0.1)
            soundspeed = 0.1;
        if (stepsbetweensound > 20) {
            stepsbetweensound = 0;
            if (m_car_sound->getIsPaused())
                m_car_sound->setIsPaused(false);
            m_car_sound->setPlaybackSpeed((irrklang::ik_f32)soundspeed);
        }
    }
#endif
}

// -----------------------------------------------------------------------------
// Render the Irrlicht scene and additional visual elements.
// -----------------------------------------------------------------------------
void ChVehicleIrrApp::DrawAll() {
    if (m_renderGrid)
        renderGrid();

    ChIrrAppInterface::DrawAll();

    if (m_renderSprings)
        renderSprings();
    if (m_renderLinks)
        renderLinks();
    if (m_renderStats)
        renderStats();
}

// Render springs in the vehicle model.
void ChVehicleIrrApp::renderSprings() {
    std::vector<chrono::ChLink*>::iterator ilink = GetSystem()->Get_linklist()->begin();
    for (; ilink != GetSystem()->Get_linklist()->end(); ++ilink) {
        if (ChLinkSpring* link = dynamic_cast<ChLinkSpring*>(*ilink)) {
            ChIrrTools::drawSpring(GetVideoDriver(), 0.05, link->GetEndPoint1Abs(), link->GetEndPoint2Abs(),
                                   video::SColor(255, 150, 20, 20), 80, 15, true);
        } else if (ChLinkSpringCB* link = dynamic_cast<ChLinkSpringCB*>(*ilink)) {
            ChIrrTools::drawSpring(GetVideoDriver(), 0.05, link->GetEndPoint1Abs(), link->GetEndPoint2Abs(),
                                   video::SColor(255, 150, 20, 20), 80, 15, true);
        }
    }
}

// render specialized joints in the vehicle model.
void ChVehicleIrrApp::renderLinks() {
    std::vector<chrono::ChLink*>::iterator ilink = GetSystem()->Get_linklist()->begin();
    for (; ilink != GetSystem()->Get_linklist()->end(); ++ilink) {
        if (ChLinkDistance* link = dynamic_cast<ChLinkDistance*>(*ilink)) {
            ChIrrTools::drawSegment(GetVideoDriver(), link->GetEndPoint1Abs(), link->GetEndPoint2Abs(),
                                    video::SColor(255, 0, 20, 0), true);
        } else if (ChLinkRevoluteSpherical* link = dynamic_cast<ChLinkRevoluteSpherical*>(*ilink)) {
            ChIrrTools::drawSegment(GetVideoDriver(), link->GetPoint1Abs(), link->GetPoint2Abs(),
                                    video::SColor(255, 180, 0, 0), true);
        }
    }
}

// Render a horizontal grid.
void ChVehicleIrrApp::renderGrid() {
    ChCoordsys<> gridCsys(ChVector<>(0, 0, m_gridHeight), chrono::Q_from_AngAxis(-CH_C_PI_2, VECT_Z));

    ChIrrTools::drawGrid(GetVideoDriver(), 0.5, 0.5, 100, 100, gridCsys, video::SColor(255, 80, 130, 255), true);
}

// Render a linear gauge in the HUD.
void ChVehicleIrrApp::renderLinGauge(const std::string& msg,
                                     double factor,
                                     bool sym,
                                     int xpos,
                                     int ypos,
                                     int length,
                                     int height) {
    irr::core::rect<s32> mclip(xpos, ypos, xpos + length, ypos + height);
    GetVideoDriver()->draw2DRectangle(irr::video::SColor(90, 60, 60, 60),
                                      irr::core::rect<s32>(xpos, ypos, xpos + length, ypos + height), &mclip);

    int left = sym ? (int)((length / 2 - 2) * std::min<>(factor, 0.0) + length / 2) : 2;
    int right = sym ? (int)((length / 2 - 2) * std::max<>(factor, 0.0) + length / 2) : (int)((length - 4) * factor + 2);

    GetVideoDriver()->draw2DRectangle(irr::video::SColor(255, 250, 200, 00),
                                      irr::core::rect<s32>(xpos + left, ypos + 2, xpos + right, ypos + height - 2),
                                      &mclip);

    irr::gui::IGUIFont* font = GetIGUIEnvironment()->getBuiltInFont();
    font->draw(msg.c_str(), irr::core::rect<s32>(xpos + 3, ypos + 3, xpos + length, ypos + height),
               irr::video::SColor(255, 20, 20, 20));
}

// Render text in a box.
void ChVehicleIrrApp::renderTextBox(const std::string& msg, int xpos, int ypos, int length, int height) {
    irr::core::rect<s32> mclip(xpos, ypos, xpos + length, ypos + height);
    GetVideoDriver()->draw2DRectangle(irr::video::SColor(90, 60, 60, 60),
                                      irr::core::rect<s32>(xpos, ypos, xpos + length, ypos + height), &mclip);

    irr::gui::IGUIFont* font = GetIGUIEnvironment()->getBuiltInFont();
    font->draw(msg.c_str(), irr::core::rect<s32>(xpos + 3, ypos + 3, xpos + length, ypos + height),
               irr::video::SColor(255, 20, 20, 20));
}

// Render stats for the vehicle and powertrain systems (render the HUD).
void ChVehicleIrrApp::renderStats() {
    char msg[100];

    sprintf(msg, "Camera mode: %s", m_camera.GetStateName().c_str());
    renderTextBox(std::string(msg), m_HUD_x, m_HUD_y, 120, 15);

    double speed = m_car.GetVehicleSpeed();
    sprintf(msg, "Speed: %+.2f", speed);
    renderLinGauge(std::string(msg), speed / 30, false, m_HUD_x, m_HUD_y + 30, 120, 15);

    double engine_rpm = m_powertrain.GetMotorSpeed() * 60 / chrono::CH_C_2PI;
    sprintf(msg, "Eng. RPM: %+.2f", engine_rpm);
    renderLinGauge(std::string(msg), engine_rpm / 7000, false, m_HUD_x, m_HUD_y + 50, 120, 15);

    double engine_torque = m_powertrain.GetMotorTorque();
    sprintf(msg, "Eng. Nm: %+.2f", engine_torque);
    renderLinGauge(std::string(msg), engine_torque / 600, false, m_HUD_x, m_HUD_y + 70, 120, 15);

    double tc_slip = m_powertrain.GetTorqueConverterSlippage();
    sprintf(msg, "T.conv. slip: %+.2f", tc_slip);
    renderLinGauge(std::string(msg), tc_slip / 1, false, m_HUD_x, m_HUD_y + 90, 120, 15);

    double tc_torquein = m_powertrain.GetTorqueConverterInputTorque();
    sprintf(msg, "T.conv. in  Nm: %+.2f", tc_torquein);
    renderLinGauge(std::string(msg), tc_torquein / 600, false, m_HUD_x, m_HUD_y + 110, 120, 15);

    double tc_torqueout = m_powertrain.GetTorqueConverterOutputTorque();
    sprintf(msg, "T.conv. out Nm: %+.2f", tc_torqueout);
    renderLinGauge(std::string(msg), tc_torqueout / 600, false, m_HUD_x, m_HUD_y + 130, 120, 15);

    int ngear = m_powertrain.GetCurrentTransmissionGear();
    ChPowertrain::DriveMode drivemode = m_powertrain.GetDriveMode();
    switch (drivemode) {
        case ChPowertrain::FORWARD:
            sprintf(msg, "Gear: forward, n.gear: %d", ngear);
            break;
        case ChPowertrain::NEUTRAL:
            sprintf(msg, "Gear: neutral");
            break;
        case ChPowertrain::REVERSE:
            sprintf(msg, "Gear: reverse");
            break;
        default:
            sprintf(msg, "Gear:");
            break;
    }
    renderLinGauge(std::string(msg), (double)ngear / 4.0, false, m_HUD_x, m_HUD_y + 150, 120, 15);

    if (ChSharedPtr<ChShaftsDriveline2WD> driveline = m_car.GetDriveline().DynamicCastTo<ChShaftsDriveline2WD>()) {
        double torque;
        int axle = driveline->GetDrivenAxleIndexes()[0];

        torque = driveline->GetWheelTorque(ChWheelID(axle, LEFT));
        sprintf(msg, "Torque wheel L: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 170, 120, 15);

        torque = driveline->GetWheelTorque(ChWheelID(axle, RIGHT));
        sprintf(msg, "Torque wheel R: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 190, 120, 15);
    } else if (ChSharedPtr<ChShaftsDriveline4WD> driveline =
                   m_car.GetDriveline().DynamicCastTo<ChShaftsDriveline4WD>()) {
        double torque;
        std::vector<int> axles = driveline->GetDrivenAxleIndexes();

        torque = driveline->GetWheelTorque(ChWheelID(axles[0], LEFT));
        sprintf(msg, "Torque wheel FL: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 210, 120, 15);

        torque = driveline->GetWheelTorque(ChWheelID(axles[0], RIGHT));
        sprintf(msg, "Torque wheel FR: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 230, 120, 15);

        torque = driveline->GetWheelTorque(ChWheelID(axles[1], LEFT));
        sprintf(msg, "Torque wheel RL: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 250, 120, 15);

        torque = driveline->GetWheelTorque(ChWheelID(axles[1], RIGHT));
        sprintf(msg, "Torque wheel FR: %+.2f", torque);
        renderLinGauge(std::string(msg), torque / 5000, false, m_HUD_x, m_HUD_y + 270, 120, 15);
    }
}

}  // end namespace chrono
