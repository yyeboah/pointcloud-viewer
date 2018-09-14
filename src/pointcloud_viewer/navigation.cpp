#include <pointcloud_viewer/navigation.hpp>
#include <pointcloud_viewer/camera.hpp>
#include <pointcloud_viewer/viewport.hpp>
#include <pointcloud_viewer/visualizations.hpp>
#include <pointcloud_viewer/usability_scheme.hpp>
#include <core_library/print.hpp>

#include <glm/gtx/io.hpp>

#include <QWidget>
#include <QApplication>
#include <QSettings>

Navigation::Navigation(Viewport* viewport)
  : viewport(viewport),
    _controller(new Controller(*this)),
    _usability_scheme(new UsabilityScheme(*_controller))
{
  connect(viewport, &Viewport::frame_rendered, this, &Navigation::updateFrameRenderDuration);

  _turntable_origin_relative_to_camera = Camera().frame.inverse() * glm::vec3(0);

  QSettings settings;
  _mouse_sensitivity_value = settings.value("Navigation/mouseSensitivity", 0).value<int>();
}

Navigation::~Navigation()
{
  stopFpsNavigation();

  QSettings settings;
  settings.setValue("Navigation/mouseSensitivity", int(_mouse_sensitivity_value));

  delete _usability_scheme;
  delete _controller;
}

void Navigation::startFpsNavigation()
{
  if(mode == Navigation::IDLE)
  {
    fps_start_frame = camera.frame;
    fps_timer = startTimer(40);
    key_direction = glm::vec3(0);
    key_speed = 0;
    num_frames_in_fps_mode = 0;
    enableMode(Navigation::FPS);
    viewport->grabMouse(Qt::BlankCursor);
    viewport->grabKeyboard();
    viewport->setMouseTracking(true);
  }
}

void Navigation::stopFpsNavigation(bool keepNewFrame)
{
  if(mode == Navigation::FPS)
  {
    if(!keepNewFrame)
    {
      camera.frame = fps_start_frame;
      viewport->update();
    }

    killTimer(fps_timer);
    fps_timer = 0;
    key_direction = glm::vec3(0);
    key_speed = 0;
    disableMode(Navigation::FPS);
    viewport->releaseKeyboard();
    viewport->releaseMouse();
    viewport->setMouseTracking(false);
  }
}

void Navigation::resetCameraLocation()
{
  camera.frame = Camera().frame;

  turntable_origin = glm::vec3(0);
  _turntable_origin_relative_to_camera = Camera().frame.inverse() * glm::vec3(0);

  viewport->update();
}

void Navigation::resetMovementSpeed()
{
  _base_movement_speed = 0;
}

void Navigation::updateFrameRenderDuration(double duration)
{
  // 0.04 because the timer limits the minimal time between to events to be 40ms anyway
  _last_frame_duration = glm::clamp(float(duration), 0.04f, 0.1f);
}

void Navigation::wheelEvent(QWheelEvent* event)
{
  if(mode == Navigation::FPS)
  {
    if(event->modifiers() == Qt::NoModifier)
      incr_base_movement_speed(event->angleDelta().y());
    else if(event->modifiers() == Qt::CTRL)
      tilt_camera(event->angleDelta().y());
    else if(event->modifiers() == Qt::CTRL+Qt::SHIFT)
      tilt_camera(event->angleDelta().y() * 4.);
  }
}

void Navigation::mouseMoveEvent(QMouseEvent* event)
{
  const glm::ivec2 current_mouse_pos(event->x(), event->y());

  bool handle_event = event->source() == Qt::MouseEventNotSynthesized;

  if(mode == Navigation::FPS)
  {
    const glm::ivec2 center = viewport_center();

    if(distance(current_mouse_pos - center, center) == VERY_FAR)
      this->set_mouse_pos(center);

    if(distance(current_mouse_pos - last_mouse_pos, center) != CLOSE)
      handle_event = false;
  }

  if(handle_event)
  {
    mouse_force = glm::vec2(current_mouse_pos - last_mouse_pos) * 0.4f * mouse_sensitivity() * _last_frame_duration;

    mouse_force = glm::clamp(glm::vec2(-20), glm::vec2(20), mouse_force);

    if(mode == Navigation::TURNTABLE_ROTATE || mode == Navigation::TURNTABLE_SHIFT || mode == Navigation::TURNTABLE_ZOOM)
    {
      navigate();
      viewport->update();
    }
  }

  last_mouse_pos = current_mouse_pos;

  event->accept();
}

void Navigation::mousePressEvent(QMouseEvent* event)
{
  if(mode == Navigation::FPS)
  {
    if(event->button() == Qt::LeftButton)
      stopFpsNavigation();
    if(event->button() == Qt::RightButton)
      stopFpsNavigation(false);
    if(event->button() == Qt::MiddleButton)
    {
      if(event->modifiers() == Qt::CTRL)
        reset_camera_tilt();
    }
  }

  if(mode == Navigation::IDLE)
  {
    if(event->button() == Qt::MiddleButton)
    {
      turntable_origin = find_best_turntable_origin();
      last_mouse_pos = glm::ivec2(event->x(), event->y());

      viewport->visualization().set_turntable_origin(find_best_turntable_origin());
      viewport->update();

      if(event->modifiers() == Qt::NoModifier)
        enableMode(Navigation::TURNTABLE_ROTATE);
      else if(event->modifiers() == Qt::ShiftModifier)
        enableMode(Navigation::TURNTABLE_SHIFT);
      else if(event->modifiers() == Qt::ControlModifier)
        enableMode(Navigation::TURNTABLE_ZOOM);
    }else if(event->button() == Qt::LeftButton)
    {
      if(event->modifiers() == Qt::NoModifier)
      {
        const glm::ivec2 screenspace_pixel = glm::ivec2(event->x(), event->y());

        simpleLeftClick(screenspace_pixel);
      }
    }
  }
}

void Navigation::mouseReleaseEvent(QMouseEvent* event)
{
  if(event->button() == Qt::MiddleButton)
  {
    disableMode(Navigation::TURNTABLE_ROTATE);
    disableMode(Navigation::TURNTABLE_SHIFT);
    disableMode(Navigation::TURNTABLE_ZOOM);
  }
}

inline glm::vec3 direction_for_key(QKeyEvent* event)
{
  glm::vec3 key_direction;
  if(event->key() == Qt::Key_W)
    key_direction.y += 1.f;
  if(event->key() == Qt::Key_Up)
    key_direction.y += 1.f;
  if(event->key() == Qt::Key_S)
    key_direction.y -= 1.f;
  if(event->key() == Qt::Key_Down)
    key_direction.y -= 1.f;
  if(event->key() == Qt::Key_A)
    key_direction.x -= 1.f;
  if(event->key() == Qt::Key_Left)
    key_direction.x -= 1.f;
  if(event->key() == Qt::Key_D)
    key_direction.x += 1.f;
  if(event->key() == Qt::Key_Right)
    key_direction.x += 1.f;
  if(event->key() == Qt::Key_E)
    key_direction.z += 1.f;
  if(event->key() == Qt::Key_Q)
    key_direction.z -= 1.f;
  return key_direction;
}

inline int speed_for_key(QKeyEvent* event)
{
  int key_speed = 0;
  if(event->key() == Qt::Key_Shift)
    key_speed++;
  return key_speed;
}

void Navigation::keyPressEvent(QKeyEvent* event)
{
  if(mode == FPS)
  {
    if(event->modifiers() == Qt::NoModifier)
    {
      if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        stopFpsNavigation();
      if(event->key() == Qt::Key_Escape)
        stopFpsNavigation(false);
    }

    if(event->modifiers() == Qt::AltModifier)
    {
      if(event->key() == Qt::Key_F4)
      {
        stopFpsNavigation();
        QApplication::quit();
      }
    }

    key_direction += direction_for_key(event);
    key_speed += speed_for_key(event);
    update_key_force();
  }
}

void Navigation::keyReleaseEvent(QKeyEvent* event)
{
  key_direction -= direction_for_key(event);
  key_speed -= speed_for_key(event);
  update_key_force();
}

void Navigation::focusOutEvent(QFocusEvent* event)
{
  Q_UNUSED(event);

  stopFpsNavigation();
}

glm::ivec2 Navigation::mouse_sensitivity_value_range() const
{
  return glm::ivec2(-100, 100);
}

int Navigation::mouse_sensitivity_value() const
{
  return _mouse_sensitivity_value;
}

void Navigation::set_mouse_sensitivity_value(int value)
{
  if(_mouse_sensitivity_value == value)
    return;

  _mouse_sensitivity_value = glm::clamp<int>(value, mouse_sensitivity_value_range()[0], mouse_sensitivity_value_range()[1]);

  mouse_sensitivity_value_changed(value);
}

void Navigation::timerEvent(QTimerEvent* timerEvent)
{
  if(timerEvent->timerId() != fps_timer || mode!=FPS)
    return;

  if(num_frames_in_fps_mode == 0)
    mouse_force = glm::vec2(0);

  navigate();

  viewport->update();

  ++num_frames_in_fps_mode;
}

glm::ivec2 Navigation::viewport_center() const
{
  QSize size = viewport->size();

  return glm::ivec2(size.width()/2,size.height()/2);
}

Navigation::distance_t Navigation::distance(glm::ivec2 difference, glm::ivec2 radius) const
{
  difference = glm::abs(difference);

  auto is_very_far = [](int value, int radius) {
    return value > radius*8/10;
  };

  auto is_far = [](int value, int radius) {
    return value > radius/2;
  };

  if(is_very_far(difference.x, radius.x) || is_very_far(difference.y, radius.y))
    return VERY_FAR;
  else if(is_far(difference.x, radius.x) || is_far(difference.y, radius.y))
    return FAR;
  else
    return CLOSE;
}

void Navigation::tilt_camera(double factor)
{
  const glm::vec3 forward = camera.frame.orientation * glm::vec3(0, 0, -1);
  const float angle = float(factor * 0.1 / (120. * glm::pi<double>()));

  glm::quat rotation = glm::angleAxis(angle, forward);

  camera.frame.orientation = rotation * camera.frame.orientation;
  viewport->update();
}

void Navigation::reset_camera_tilt()
{
  camera.frame = remove_tilt(camera.frame);
  viewport->update();
}

void Navigation::incr_base_movement_speed(int incr)
{
  _base_movement_speed = glm::clamp(incr+_base_movement_speed, -6000-1200, 6000-1200);
}

float Navigation::mouse_sensitivity() const
{
  return glm::pow(1.03f, float(_mouse_sensitivity_value));
}

glm::vec3 Navigation::find_best_turntable_origin()
{
  aabb_t aabb = viewport->aabb();

  glm::vec3 v = camera.frame * _turntable_origin_relative_to_camera;

  if(aabb.is_valid())
    v = glm::clamp(v, aabb.min_point, aabb.max_point);

  return v;
}

float Navigation::base_movement_speed() const
{
  return glm::pow(1.01f, float(_base_movement_speed) / 15.f);
}

void Navigation::update_key_force()
{
  if(glm::length(key_direction) > 0.5f)
    key_force = glm::normalize(key_direction) * glm::exp2(glm::clamp<float>(-1, 1, key_speed)) * 0.5f;
  else
    key_force = glm::vec3(0.f);
}

void Navigation::navigate()
{
  if(mode == IDLE)
    return;

  frame_t& view = camera.frame;

  const glm::vec3 forward = view.orientation * glm::vec3(0, 0, -1);
  const glm::vec3 up = view.orientation * glm::vec3(0, 1, 0);
  const glm::vec3 right = view.orientation * glm::vec3(1, 0, 0);

  switch(mode)
  {
  case FPS:
  {
    const glm::vec3 movement =  up * key_force.z + forward * key_force.y + right * key_force.x;

    view.orientation = glm::angleAxis(-mouse_force.x, glm::vec3(0,0,1)) * glm::angleAxis(-mouse_force.y, right) * view.orientation;

    view.position += movement * base_movement_speed();
//    turntable_origin += movement;
    break;
  }
  case TURNTABLE_ROTATE:
  {
    const float factor = 0.5f;
    view.position -= turntable_origin;
    view = frame_t(turntable_origin, glm::angleAxis(factor * -mouse_force.x, glm::vec3(0,0,1)) * glm::angleAxis(factor * -mouse_force.y, right)) * view;
    break;
  }
  case TURNTABLE_SHIFT:
  {
    const float factor = 0.5f;
    const glm::vec3 shift = up * mouse_force.y - right * mouse_force.x;
    view.position += factor * shift;
    turntable_origin += shift;
    break;
  }
  case TURNTABLE_ZOOM:
  {
    const float factor = 0.5f;
    glm::vec3 previous_zoom = view.position - turntable_origin;

    float zoom_factor = glm::clamp(0.5f, 1.5f, glm::exp2(factor * mouse_force.y));

    if(zoom_factor * length(previous_zoom) > 1.e-2f)
      view.position = turntable_origin + zoom_factor * previous_zoom;
    break;
  }
  case IDLE:
    break;
  }

  mouse_force = glm::vec2(0);
}

void Navigation::enableMode(Navigation::mode_t mode)
{
  if(this->mode == IDLE)
  {
    this->mode = mode;
  }
}

void Navigation::disableMode(Navigation::mode_t mode)
{
  if(this->mode == mode)
  {
    switch(this->mode)
    {
    case TURNTABLE_ZOOM:
      _turntable_origin_relative_to_camera = camera.frame.inverse() * turntable_origin;
      break;
    case FPS:
    case TURNTABLE_SHIFT:
    case IDLE:
    case TURNTABLE_ROTATE:
      break;
    }

    if(mode != TURNTABLE_ZOOM)
    {
      viewport->visualization().set_turntable_origin(find_best_turntable_origin());
      viewport->update();
    }

    this->mode = IDLE;
  }
}

void Navigation::set_mouse_pos(glm::ivec2 mouse_pos)
{
  QCursor cursor = viewport->cursor();
  cursor.setPos(viewport->mapToGlobal(QPoint(mouse_pos.x, mouse_pos.y)));
  viewport->setCursor(cursor);
}

Navigation::Controller::Controller(Navigation& navigation)
  : navigation(navigation)
{
}
