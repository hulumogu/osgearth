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

#include <osgEarth/TileCache>
#include <osgEarth/PlateCarre>
#include <osgEarth/TileSource>
#include <osgEarth/FileUtils>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

using namespace osgEarth;

DiskCache::DiskCache(const std::string &path, const std::string format)
{
    _path = path;
    _format = format;
}


bool DiskCache::existsInCache(const TileKey* key, const TileSource* source)
{
    return osgDB::fileExists(getFileName(key, source));
}

osg::Image* DiskCache::getImage(const TileKey* key, const TileSource* source)
{
    std::string filename = getFileName(key, source);
    //osg::notify(osg::NOTICE) << "DiskCache: getImage " << filename << std::endl;
    return osgDB::readImageFile( filename );
}

std::string DiskCache::getPath()
{
    //Return early if the cache path is empty
    if (_path.empty()) return _path;
    return getFullPath(_mapConfigFilename, _path);
}

void DiskCache::setImage(const TileKey* key, const TileSource* source, const osg::Image* image)
{
    std::string filename = getFileName(key, source);
    std::string path = osgDB::getFilePath(filename);
    //If the path doesn't currently exist or we can't create the path, don't cache the file
    if (!osgDB::fileExists(path) && !osgDB::makeDirectory(path))
    {
        osg::notify(osg::WARN) << "Couldn't create path " << path << std::endl;
    }
    osgDB::writeImageFile(*image, filename);
}

std::string DiskCache::getFormat(const TileSource* source)
{
     //If the format is empty, use the format of the TileSource.  If the TileSource doesn't report a format, default to png 
    std::string format = _format;
    if (format.empty()) format = source->getExtension();
    if (format.empty()) format = "png";
    return format;
}

std::string DiskCache::getFileName(const TileKey* key, const TileSource* source)
{
    unsigned int level, x, y;
    level = key->getLevelOfDetail();
    key->getTileXY( x, y );

    // need to invert the y-tile index
    y = key->getMapSizeTiles() - y - 1;

    char buf[2048];
    sprintf( buf, "%s/%s/%02d/%03d/%03d/%03d/%03d/%03d/%03d.%s",
        getPath().c_str(),
        source->getName().c_str(),
        level,
        (x / 1000000),
        (x / 1000) % 1000,
        (x % 1000),
        (y / 1000000),
        (y / 1000) % 1000,
        (y % 1000),
        getFormat(source).c_str());
    return buf;
}


TMSCache::TMSCache(const std::string &path, const std::string format):
 DiskCache(path, format),
 _invertY(false)
{
}


std::string TMSCache::getFileName(const TileKey* key, const TileSource* source)
{
    unsigned int x,y;
    key->getTileXY(x, y);

    if (!_invertY)
    {
        y = key->getMapSizeTiles() - y - 1;
    }
    
    unsigned int lod = key->getLevelOfDetail();
    if (dynamic_cast<const PlateCarreTileKey*>(key))
    {
        lod--;
    }
    std::stringstream buf;
    buf << getPath() << "/" << source->getName() << "/" << lod << "/" << x << "/" << y << "." << getFormat(source);
    return buf.str();
}

QuadKeyCache::QuadKeyCache(const std::string &path, const std::string format):DiskCache(path, format)
{
}


std::string QuadKeyCache::getFileName(const TileKey* key, const TileSource* source)
{
    std::stringstream buf;
    buf << getPath() << "/" << source->getName() << "/" << key->str() << "." << getFormat(source);
    return buf.str();
}

static bool getProp(const std::map<std::string,std::string> &map, const std::string &key, std::string &value)
{
    std::map<std::string,std::string>::const_iterator i = map.find(key);
    if (i != map.end())
    {
        value = i->second;
        return true;
    }
    return false;
}


TileCache* TileCacheFactory::create(const std::string &type, std::map<std::string,std::string> properties)
{
    if (type == "tms" || type == "tilecache" || type == "quadkey" || type.empty())
    {
        std::string path;
        std::string format;

        if ((!getProp(properties, "path", path)) || (path.empty()))
        {
            osg::notify(osg::NOTICE) << "No path specified for " << type << " cache " << std::endl;
        }

        getProp(properties, "format", format);
        
        if (type == "tms")
        {
            TMSCache *cache = new TMSCache(path, format);
            std::string tms_type; 
            getProp(properties, "tms_type", tms_type);
            if (tms_type == "google")
            {
                osg::notify(osg::INFO) << "Inverting Y in TMS cache " << std::endl;
                cache->setInvertY(true);
            }
            osg::notify(osg::INFO) << "Returning TMS cache " << std::endl;
            return cache;
        }

        if (type == "tilecache" || type.empty())
        {
            osg::notify(osg::INFO) << "Returning disk cache " << std::endl;
            return new DiskCache(path, format);
        }

        if (type == "quadkey")
        {
            osg::notify(osg::INFO) << "Returning quadkey cache " << std::endl;
            return new QuadKeyCache(path, format);
        }
    }
    else if (type == "none")
    {
        osg::notify(osg::INFO) << "Returning null cache " << std::endl;
        return new NullCache();
    }
    return 0;
}