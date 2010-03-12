/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <osgEarthSymbology/MarkerSymbolizer>
#include <osgEarthSymbology/MarkerSymbol>
#include <osgEarthFeatures/Feature>
#include <osgDB/ReadFile>
#include <osgDB/ReaderWriter>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/Geode>
#include <osg/Version>


using namespace osgEarth::Symbology;
using namespace osgEarth::Features;


static osg::Node* getNode(const std::string& str)
{
#if OSG_VERSION_LESS_THAN(2,9,0)
    osg::ref_ptr<osgDB::ReaderWriter::Options> options = new osgDB::Options;
    options->setObjectCacheHint(osgDB::ReaderWriter::Options::CACHE_ALL);
    osg::Node* node = osgDB::readNodeFile(str, options.get());
    return node;
#else
    osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
    options->setObjectCacheHint(osgDB::Options::CACHE_ALL);
    osg::Node* node = osgDB::readNodeFile(str, options.get());
    return node;
#endif
}

MarkerSymbolizer::MarkerSymbolizer()
{
}


bool 
MarkerSymbolizer::update(FeatureDataSet* dataSet,
                         const osgEarth::Symbology::Style* style,
                         osg::Group* attachPoint,
                         SymbolizerContext* context )
{
    if (!dataSet || !attachPoint || !style)
        return false;

    osg::ref_ptr<osgEarth::Features::FeatureCursor> cursor = dataSet->createCursor();
    if (!cursor)
        return false;

    osg::ref_ptr<osg::Group> newSymbolized = new osg::Group;

    osgEarth::Features::Feature* feature = 0;
    while( cursor->hasMore() )
    {
        feature = cursor->nextFeature();
        if (!feature || !feature->getGeometry())
            continue;

        Geometry* geometry = feature->getGeometry();

        GeometryIterator geomIterator( geometry );
        geomIterator.traverseMultiGeometry() = true;
        geomIterator.traversePolygonHoles() = true;

        while( geomIterator.hasMore() )
        {
            Geometry* part = geomIterator.next();
            if (!part)
                continue;

            switch( part->getType())
            {
            case Geometry::TYPE_POINTSET:
                if (style->getPoint())
                {
                    const MarkerSymbol* point = dynamic_cast<const MarkerSymbol*>(style->getPoint());
                    if (point && part->size() && !point->marker().value().empty())
                    {
                        osg::Node* node = getNode(point->marker().value());
                        if (!node) {
                            osg::notify(osg::WARN) << "can't load Marker Node " << point->marker().value() << std::endl;
                            continue;
                        }
                        osg::Group* group = new osg::Group;
                        for ( osg::Vec3dArray::iterator it = part->begin(); it != part->end(); ++it)
                        {
                            osg::Vec3d pos = *it;
                            osg::MatrixTransform* tr = new osg::MatrixTransform;
                            tr->setMatrix(osg::Matrix::translate(pos));
                            tr->addChild(node);
                            group->addChild(tr);
                        }
                        newSymbolized->addChild(group);
                    }
                }
                break;

            case Geometry::TYPE_LINESTRING:
            case Geometry::TYPE_RING:
                if (style->getLine())
                {
                    const MarkerLineSymbol* line = dynamic_cast<const MarkerLineSymbol*>(style->getLine());
                    if (line && part->size() && !line->marker().value().empty())
                    {
                        osg::Node* node = getNode(line->marker().value());
                        if (!node) {
                            osg::notify(osg::WARN) << "can't load Marker Node " << line->marker().value() << std::endl;
                            continue;
                        }
                        float interval = line->interval().value();
                        if (!interval)
                            interval = 1.0;


                        osg::Group* group = new osg::Group;
                        float globalDist = 0;
                        float currentDist = 0;

                        // start to put one first node
                        {
                        osg::MatrixTransform* tr = new osg::MatrixTransform;
                        tr->setMatrix(osg::Matrix::translate(*part->begin()));
                        tr->addChild(node);
                        group->addChild(tr);
                        }

                        for ( int i = 0; i < part->size(); ++i)
                        {
                            osg::Vec3d start = (*part)[i];
                            osg::Vec3d end;
                            if (i < (part->size() - 1)) {
                                end = (*part)[i+1];
                            } else {
                                if (part->getType() == Geometry::TYPE_RING) {
                                    end = (*part)[0];
                                } else {
                                    end = start;
                                }
                            }
                            osg::Vec3d direction = end - start;
                            float segmentSize = direction.length();
                            direction.normalize();

                            float previousDist = currentDist;
                            currentDist += segmentSize;
                            if (currentDist < interval)
                                continue;
                                
                            float offset = interval - previousDist;
                                
                            float rate = currentDist / interval;
                            int nbItems = static_cast<int>(floorf(rate));
                            rate -= (float)nbItems;
                            float remaining = rate * interval;
                            currentDist = remaining;

                            osg::Vec3d startOnTheLine = start + direction * offset;
                            for (int j = 0; j < nbItems; ++j)
                            {
                                osg::MatrixTransform* tr = new osg::MatrixTransform;
                                tr->setMatrix(osg::Matrix::translate(startOnTheLine + direction*j*interval));
                                tr->addChild(node);
                                group->addChild(tr);
                            }
                        }
                        newSymbolized->addChild(group);
                    }
                }
                break;

            case Geometry::TYPE_POLYGON:
                break;

            }
        }
    }

    if (newSymbolized->getNumChildren())
    {
        attachPoint->removeChildren(0, attachPoint->getNumChildren());
        attachPoint->addChild(newSymbolized.get());
        return true;
    }

    return false;
}