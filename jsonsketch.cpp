#include <QJsonArray>
#include <QList>
#include <QVector2D>
#include <QJsonObject>
#include <QString>
#include <QFile>
#include <QByteArray>
#include <QQuickItem>
#include <QJsonDocument>
#include <QString>
#include <QQmlContext>
#include <QQmlComponent>

#include "jsonsketch.h"

JSONSketch::JSONSketch(QObject *parent): QObject(parent), nextPointId(0),
    nextLineId(0), nb_points(0), nb_lines(0) {}

QString JSONSketch::loadSketch(const QString url, QObject* sketch)
{
    QFile file(url);

    if (!file.open(QIODevice::ReadOnly)) {
        return "cannot open the JSON file";
    }

    QByteArray data = file.readAll();

    QJsonParseError error;
    QJsonDocument doc(QJsonDocument::fromJson(data, &error));

    return read(doc.object(), sketch);
}

QString JSONSketch::read(const QJsonObject json, QObject* sketch)
{
    qDebug()<<json;

    int json_sketch_width=json["sketch_width"].toInt();
    int json_sketch_height=json["sketch_height"].toInt();

    qreal scale_w=(qreal)sketch->property("width").toInt()/json_sketch_width;
    qreal scale_h=(qreal)sketch->property("height").toInt()/json_sketch_height;

    QJsonArray qPid = json["pid"].toArray();
    QJsonArray qLid = json["lid"].toArray();
    QJsonArray qCid = json["cid"].toArray();

    QJsonObject qPoints = json["points"].toObject();
    QJsonObject qLines = json["lines"].toObject();
    QJsonObject qConstraints = json["constraints"].toObject();

    qDebug() << "qPid : " << qPid;
    qDebug() << "qLid : " << qLid;
    qDebug() << "qCid : " << qCid;

    qDebug() << "qPoints : " << qPoints;
    qDebug() << "qLines : " << qLines;
    qDebug() << "qConstraints : " << qConstraints;

    nb_points = json["nb_points"].toInt();
    nb_lines = json["nb_lines"].toInt();
    nb_constraints = json["nb_constraints"].toInt();

    qDebug() << "nb_points : " << nb_points;
    qDebug() << "nb_lines : " << nb_lines;
    qDebug() << "nb_constraints : " << nb_constraints;

    if(nb_points != qPoints.size() || nb_lines != qLines.size() || nb_constraints != qConstraints.size()) {
        return "corrupted number of points, lines or constraints";
    }

    QJsonArray currPoint;
    for(int p(0); p < nb_points; p++){
        int intPid = qPid[p].toInt();
        currPoint = qPoints[QString::number(intPid)].toArray();
        if (currPoint.size() != 2){
            return "corrupted point with id: " + QString::number(intPid);
        }
        points.insert(intPid, QVector2D(currPoint[0].toInt()*scale_w, currPoint[1].toInt()*scale_h));
    }

    QJsonArray currLine;
    for(int l(0); l < nb_lines; l++){
        int intLid = qLid[l].toInt();
        currLine = qLines[QString::number(intLid)].toArray();
        if (currLine.size() != 2){
            return "corrupted line with id: " + QString::number(intLid);
        }
        if(currLine[0] == currLine[1] ||
                !points.contains(currLine[0].toInt()) ||
                !points.contains(currLine[1].toInt())) {
            return "corrupted point id of line with id: ";
        }
        lines.insert(intLid, QVector2D(currLine[0].toInt(), currLine[1].toInt()));
    }

    QJsonArray currConst;
    for(int c(0); c < nb_constraints; c++){
        int intCid = qCid[c].toInt();
        currConst = qConstraints[QString::number(intCid)].toArray();
        if (currConst.size() != 6){
            return "corrupted constraints with id: " + QString::number(intCid);
        }
        constraints.insert(intCid, QList<int>(
        {currConst[0].toInt(), currConst[1].toInt(),
         currConst[2].toInt(), currConst[3].toInt(),
         currConst[4].toInt(), currConst[5].toInt()}));
    }

    generateSketch(sketch);

    return "true";
}

void JSONSketch::generateSketch(QObject* sketch) {
    QQmlComponent point_component(qmlEngine(sketch),sketch);
    point_component.loadUrl(QUrl("qrc:/Point.qml"));

    QMap<int, QObject*> qPoints;

    foreach (int pid, points.keys()) {
        QQmlContext* point_context = new QQmlContext(qmlContext(sketch),sketch);
        QQuickItem* point = qobject_cast<QQuickItem*>(point_component.create(point_context));

        point_context->setContextObject(point);
        point->setProperty("x", points[pid].x());
        point->setProperty("y", points[pid].y());

        point->setParent(sketch);
        point->setParentItem(qobject_cast<QQuickItem*>(sketch));
        point_component.completeCreate();

        qPoints.insert(pid, point);
    }

    QQmlComponent line_component(qmlEngine(sketch),sketch);
    line_component.loadUrl(QUrl("qrc:/Line.qml"));

    QMap<int, QObject*> qLines;

    foreach (int lid, lines.keys()) {
        QQmlContext* line_context = new QQmlContext(qmlContext(sketch),sketch);
        QQuickItem* line = qobject_cast<QQuickItem*>(line_component.beginCreate(line_context));

        line_context->setContextObject(line);

        line->setProperty("p1", qVariantFromValue(qPoints[lines[lid].x()]));
        line->setProperty("p2", qVariantFromValue(qPoints[lines[lid].y()]));

        line->setParent(sketch);
        line->setParentItem(qobject_cast<QQuickItem*>(sketch));
        line_component.completeCreate();

        qLines.insert(lid, line);
    }

    QQmlComponent const_component(qmlEngine(sketch),sketch);
    const_component.loadUrl(QUrl("qrc:/Constraint.qml"));

    foreach (int cid, constraints.keys()) {
        QQmlContext* const_context = new QQmlContext(qmlContext(sketch),sketch);
        QQuickItem* constraint = qobject_cast<QQuickItem*>(const_component.beginCreate(const_context));

        const_context->setContextObject(constraint);

        constraint->setParent(sketch);
        constraint->setParentItem(qobject_cast<QQuickItem*>(sketch));
        const_component.completeCreate();

        constraint->setProperty("type", constraints[cid][0]);
        constraint->setProperty("valA", constraints[cid][1]);
        constraint->setProperty("ptA", qVariantFromValue(constraints[cid][2] == -1 ? nullptr : qPoints[constraints[cid][2]]));
        constraint->setProperty("ptB", qVariantFromValue(constraints[cid][3] == -1 ? nullptr : qPoints[constraints[cid][3]]));
        constraint->setProperty("entityA", qVariantFromValue(constraints[cid][4] == -1 ? nullptr : qLines[constraints[cid][4]]));
        constraint->setProperty("entityB", qVariantFromValue(constraints[cid][5] == -1 ? nullptr : qLines[constraints[cid][5]]));
    }
}

QString JSONSketch::exportJSONSketch(const QString url, QObject* sketch) {

    QJsonObject object;
    if(!write(object, sketch)){
        return "Empty skecth";
    }

    QFile file(url);

    if (!file.open(QIODevice::WriteOnly)) {
        return "cannot open the JSON file";
    }

    QJsonDocument doc(object);
    file.write(doc.toJson());
    file.close();

    return "true";
}

bool JSONSketch::write(QJsonObject &json, QObject* sketch)
{
    nextPointId = 0;
    nextLineId = 0;
    nextConstraintId = 0;
    nb_points = 0;
    nb_lines = 0;
    nb_constraints = 0;

    QJsonObject qPoints;
    QJsonArray qPid;

    QJsonObject qLines;
    QJsonArray qLid;

    QJsonObject qConstraints;
    QJsonArray qCid;

    foreach (QObject* child, sketch->children()) {
        if (!QString::compare(child->property("class_type").toString(), "Point")
                && child->property("existing").toBool()) {
            int id = addPoint(child->property("x").toInt(), child->property("y").toInt());
            child->setProperty("id", id);
            qPid.append(id);
            QJsonArray currPoint;
            currPoint.append(child->property("x").toInt());
            currPoint.append(child->property("y").toInt());
            qPoints.insert(QString::number(id), currPoint);
        }
    }
    foreach (QObject* child, sketch->children()) {
        if (!QString::compare(child->property("class_type").toString(), "Line")
                && child->property("existing").toBool()) {
            QObject* p1 = qvariant_cast<QObject*>(child->property("p1"));
            QObject* p2 = qvariant_cast<QObject*>(child->property("p2"));
            if(p1 != nullptr && p2 !=nullptr){
                int id = addLine(p1->property("id").toInt(), p2->property("id").toInt());
                child->setProperty("id", id);
                qLid.append(id);
                QJsonArray currLine;
                currLine.append(p1->property("id").toInt());
                currLine.append(p2->property("id").toInt());
                qLines.insert(QString::number(id), currLine);
            } else {
                child->setProperty("existing", false);
            }
        }
    }

    foreach (QObject* child, sketch->children()) {
        if (!QString::compare(child->property("class_type").toString(), "Constraint")
                && child->property("existing").toBool()) {
            int type = qvariant_cast<int>(child->property("type"));
            double valA = qvariant_cast<double>(child->property("valA"));
            QObject* ptA = qvariant_cast<QObject*>(child->property("ptA"));
            QObject* ptB = qvariant_cast<QObject*>(child->property("ptB"));
            QObject* entityA = qvariant_cast<QObject*>(child->property("entityA"));
            QObject* entityB = qvariant_cast<QObject*>(child->property("entityB"));

            int id = addConstraint(type, valA,
                                   ptA == nullptr ? -1 : ptA->property("id").toInt(),
                                   ptB == nullptr ? -1 : ptB->property("id").toInt(),
                                   entityA == nullptr ? -1 : entityA->property("id").toInt(),
                                   entityB == nullptr ? -1 : entityB->property("id").toInt());
            child->setProperty("id", id);
            qCid.append(id);
            QJsonArray currConst;
            currConst.append(type);
            currConst.append(valA);
            ptA == nullptr ? currConst.append(-1) : currConst.append(ptA->property("id").toInt());
            ptB == nullptr ? currConst.append(-1) : currConst.append(ptB->property("id").toInt());
            entityA == nullptr ? currConst.append(-1) : currConst.append(entityA->property("id").toInt());
            entityB == nullptr ? currConst.append(-1) : currConst.append(entityB->property("id").toInt());
            qConstraints.insert(QString::number(id), currConst);
        }
    }

    json["sketch_width"] = sketch->property("width").toInt();
    json["sketch_height"] = sketch->property("height").toInt();

    json["pid"] = qPid;
    json["points"] = qPoints;

    json["lid"] = qLid;
    json["lines"] = qLines;

    json["cid"] = qCid;
    json["constraints"] = qConstraints;

    json["nb_points"] = nb_points;
    json["nb_lines"] = nb_lines;
    json["nb_constraints"] = nb_constraints;

    return nb_points > 0;
}

int JSONSketch::addPoint(int x, int y) {
    int id = incrementPointsId();
    points.insert(id, QVector2D(x, y));
    nb_points++;
    return id;
}

int JSONSketch::addLine(int p1, int p2) {
    int id = incrementLinesId();
    lines.insert(id, QVector2D(p1, p2));
    nb_lines++;
    return id;
}

int JSONSketch::addConstraint(int type, int valA, int ptA, int ptB, int entityA, int entityB) {
    int id = incrementConstraintId();
    constraints.insert(id, QList<int>({type, valA, ptA, ptB, entityA, entityB}));
    nb_constraints++;
    return id;
}

int JSONSketch::incrementPointsId() {
    return nextPointId++;
}

int JSONSketch::incrementLinesId() {
    return nextLineId++;
}

int JSONSketch::incrementConstraintId() {
    return nextConstraintId++;
}
