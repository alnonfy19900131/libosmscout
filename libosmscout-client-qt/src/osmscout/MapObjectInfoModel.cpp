
/*
 OSMScout - a Qt backend for libosmscout and libosmscout-map
 Copyright (C) 2016  Lukas Karas

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <QtCore/qabstractitemmodel.h>

#include <osmscout/MapObjectInfoModel.h>
#include <osmscout/OSMScoutQt.h>
#include <iostream>

namespace osmscout {

MapObjectInfoModel::MapObjectInfoModel():
ready(false), setup(false), view(), lookupModule(nullptr)
{

  lookupModule=OSMScoutQt::GetInstance().MakeLookupModule();
  this->mapDpi=OSMScoutQt::GetInstance().GetSettings()->GetMapDPI();

  connect(lookupModule, SIGNAL(initialisationFinished(const DatabaseLoadedResponse)),
          this, SLOT(dbInitialized(const DatabaseLoadedResponse)),
          Qt::QueuedConnection);

  connect(this, SIGNAL(objectsOnViewRequested(const MapViewStruct&)),
          lookupModule, SLOT(requestObjectsOnView(const MapViewStruct&)),
          Qt::QueuedConnection);

  connect(lookupModule, SIGNAL(viewObjectsLoaded(const MapViewStruct&, const osmscout::MapData&)),
          this, SLOT(onViewObjectsLoaded(const MapViewStruct&, const osmscout::MapData&)),
          Qt::QueuedConnection);

  connect(this, SIGNAL(objectsRequested(const LocationEntry &)),
          lookupModule, SLOT(requestObjects(const LocationEntry&)),
          Qt::QueuedConnection);

  connect(lookupModule, SIGNAL(objectsLoaded(const LocationEntry&, const osmscout::MapData&)),
          this, SLOT(onObjectsLoaded(const LocationEntry&, const osmscout::MapData&)),
          Qt::QueuedConnection);
}

MapObjectInfoModel::~MapObjectInfoModel()
{
  if (lookupModule!=nullptr){
    lookupModule->deleteLater();
    lookupModule=nullptr;
  }
}

void MapObjectInfoModel::dbInitialized(const DatabaseLoadedResponse&)
{
  if (setup){
    emit objectsOnViewRequested(view);
  }
}

Qt::ItemFlags MapObjectInfoModel::flags(const QModelIndex &index) const
{
  if(!index.isValid()) {
      return Qt::ItemIsEnabled;
  }

  return QAbstractListModel::flags(index) | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> MapObjectInfoModel::roleNames() const
{
  QHash<int, QByteArray> roles=QAbstractListModel::roleNames();

  roles[LabelRole] ="label";
  roles[TypeRole]  ="type";
  roles[IdRole]    ="id";
  roles[NameRole]  ="name";
  roles[ObjectRole]="object";

  return roles;
}

QObject* MapObjectInfoModel::createOverlayObject(int row) const
{
  OverlayObject *o=nullptr;
  if(row < 0 || row >= model.size()) {
    qDebug() << "Undefined row" << row;
    return o;
  }
  const ObjectInfo &obj = model.at(row);
  if (!obj.points.empty()){
    if (obj.type=="node"){
      o=new OverlayNode();
    } else if (obj.type=="way"){
      o=new OverlayWay();
    } else if (obj.type=="area"){
      o=new OverlayArea();
    }
    if (o!=nullptr) {
      for (auto const p:obj.points) {
        o->addPoint(p.GetLat(), p.GetLon());
      }
    }
  }

  return o;
}

QVariant MapObjectInfoModel::data(const QModelIndex &index, int role) const
{
  //qDebug() << "Get data" << index.row() << " role: " << role;

  if(index.row() < 0 || index.row() >= model.size()) {
    qDebug() << "Undefined row" << index.row();
    return QVariant();
  }
  const ObjectInfo &obj = model.at(index.row());

  if (role==LabelRole){
    return QVariant::fromValue(obj.type);
  }
  if (role==TypeRole){
    return QVariant::fromValue(obj.objectType);
  }
  if (role==IdRole){
    return QVariant::fromValue(obj.id);
  }
  if (role==NameRole){
    if (obj.name.isEmpty())
      return QVariant();
    return QVariant::fromValue(obj.name);
  }

  if (role==ObjectRole){
    return QVariant::fromValue(createOverlayObject(index.row()));
  }

  //qDebug() << "Undefined role" << role << "("<<LabelRole<<"..."<<NameRole<<")";
  return QVariant();
}

void MapObjectInfoModel::setPosition(QObject *o,
                                     const int width, const int height,
                                     const int screenX, const int screenY)
{
  MapView *mapView = dynamic_cast<MapView*>(o);
  if (mapView ==nullptr){
      qWarning() << "Failed to cast " << o << " to MapView*.";
      return;
  }
  MapViewStruct r;
  r.angle=mapView->angle;
  r.coord=mapView->center;
  r.width=width;
  r.height=height;
  r.magnification=mapView->magnification;
  r.dpi=mapDpi;

  this->screenX=screenX;
  this->screenY=screenY;
  this->ready=false;
  emit readyChange(ready);

  if (this->view!=r){
    this->view=r;
    beginResetModel();
    model.clear();
    mapData.clear();
    endResetModel();
    emit objectsOnViewRequested(view);
  }else{
    update();
  }
  setup=true;
}

void MapObjectInfoModel::setLocationEntry(QObject *o)
{
  LocationEntry *location = dynamic_cast<LocationEntry*>(o);
  if (location ==nullptr){
    qWarning() << "Failed to cast " << o << " to LocationEntry*.";
    return;
  }
  locationEntry=*location;

  beginResetModel();
  model.clear();
  mapData.clear();
  endResetModel();

  this->ready=false;
  emit readyChange(ready);
  emit objectsRequested(*location);
}

void MapObjectInfoModel::onObjectsLoaded(const LocationEntry &entry, const osmscout::MapData& data)
{
  if (locationEntry.getDatabase()!=entry.getDatabase() ||
      locationEntry.getReferences()!=entry.getReferences()){
    return; // ignore
  }

  mapData << data;
  update();
}

void MapObjectInfoModel::onViewObjectsLoaded(const MapViewStruct &view, const osmscout::MapData &data)
{
  if (this->view!=view){
    return;
  }
  mapData << data;
  update();
}

void MapObjectInfoModel::update()
{
  osmscout::MercatorProjection projection;
  projection.Set(view.coord, /* angle */ 0, view.magnification, mapDpi, view.width, view.height);
  projection.SetLinearInterpolationUsage(view.magnification.GetLevel() >= 10);

  beginResetModel();
  model.clear();
  //std::cout << "object near " << this->screenX << " " << this->screenY << ":" << std::endl;

  double x;
  double y;
  double x2;
  double y2;
  double tolerance=mapDpi/4;
  QRectF rectangle(this->screenX-tolerance, this->screenY-tolerance, tolerance*2, tolerance*2);
  for (auto const &d:mapData){

    //std::cout << "nodes: " << d.nodes.size() << std::endl;
    for (auto const &n:d.nodes){
      projection.GeoToPixel(n->GetCoords(),x,y);
      if (rectangle.contains(x,y)){
        std::vector<osmscout::Point> nodes;
        nodes.emplace_back(0, n->GetCoords());

        addObjectInfo("node",
                      n->GetObjectFileRef().GetFileOffset(),
                      nodes,
                      n);
      }
    }

    //std::cout << "ways:  " << d.ways.size() << std::endl;
    for (auto const &w:d.ways){
      // TODO: better detection
      osmscout::GeoBox bbox=w->GetBoundingBox();
      projection.GeoToPixel(bbox.GetMinCoord(),x,y);
      projection.GeoToPixel(bbox.GetMaxCoord(),x2,y2);
      if (rectangle.intersects(QRectF(QPointF(x,y),QPointF(x2,y2)))){
        addObjectInfo("way",
                      w->GetObjectFileRef().GetFileOffset(),
                      w->nodes,
                      w);
      }
    }

    //std::cout << "areas: " << d.areas.size() << std::endl;
    for (auto const &a:d.areas){
      // TODO: better detection
      osmscout::GeoBox bbox=a->GetBoundingBox();
      projection.GeoToPixel(bbox.GetMinCoord(),x,y);
      projection.GeoToPixel(bbox.GetMaxCoord(),x2,y2);
      if (rectangle.intersects(QRectF(QPointF(x,y),QPointF(x2,y2)))){
        for (const auto &ring:a->rings) {
          if (!ring.GetType()->GetIgnore()) {
            addObjectInfo("area",
                          a->GetObjectFileRef().GetFileOffset(),
                          ring.nodes,
                          &ring);
          }
        }
      }
    }
  }
  //std::cout << "count: "<< model.size() << std::endl;
  endResetModel();

  this->ready=true;
  emit readyChange(ready);
}
}
