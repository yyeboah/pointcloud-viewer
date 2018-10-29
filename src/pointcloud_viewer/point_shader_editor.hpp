﻿#ifndef POINT_SHADER_HPP
#define POINT_SHADER_HPP

#include <pointcloud/pointcloud.hpp>

#include <QDialogButtonBox>
#include <QSharedPointer>
#include <QWidget>
#include <QToolButton>
#include <memory>

namespace QtNodes
{

struct DataModelRegistry;
struct FlowScene;
struct FlowView;

} // namespace QtNodes

class MainWindow;

class PointShaderEditor : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(bool isPointCloudLoaded READ isPointCloudLoaded NOTIFY isPointCloudLoadedChanged)
  Q_PROPERTY(bool isReadOnly READ isReadOnly WRITE setIsReadOnly NOTIFY isReadOnlyChanged)
public:
  PointShaderEditor(MainWindow* mainWindow);
  ~PointShaderEditor();

  void unload_all_point_clouds();
  void load_point_cloud(QSharedPointer<PointCloud> point_cloud);

  PointCloud::Shader autogenerate() const;

  bool isPointCloudLoaded() const;
  bool isReadOnly() const;

public slots:
  void setIsReadOnly(bool isReadOnly);

signals:
  void isPointCloudLoadedChanged(bool isPointCloudLoaded);

  void isReadOnlyChanged(bool isReadOnly);

private:
  friend QSet<QString> find_used_properties(const QSharedPointer<const PointCloud>& pointcloud);
  friend PointCloud::Shader generate_code_from_shader(const QSharedPointer<const PointCloud>& pointcloud);

  MainWindow& mainWindow;

  QSharedPointer<PointCloud>  _pointCloud;
  QtNodes::FlowScene* flowScene = nullptr;

  QtNodes::FlowView* flowView = nullptr;
  QtNodes::FlowScene* fallbackFlowScene = nullptr;
  QAction* importShader_action = nullptr;
  QAction* exportShader_action = nullptr;

  static std::shared_ptr<QtNodes::DataModelRegistry> qt_nodes_model_registry(const QSharedPointer<const PointCloud>& pointcloud);

  bool m_isReadOnly = false;

private slots:
  void applyShader();
  void closeEditor();
  void importShader();
  void exportShader();
};

QSet<QString> find_used_properties(const QSharedPointer<const PointCloud>& pointcloud);
PointCloud::Shader generate_code_from_shader(const QSharedPointer<const PointCloud>& pointcloud);
PointCloud::Shader generate_code_from_shader(QtNodes::FlowScene* flowScene, PointCloud::Shader shader);


#endif // POINT_SHADER_HPP
