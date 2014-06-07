/******************************************************************************
 * $Id: ogrgeojsonreader.h 23325 2013-11-05 17:19:38Z rouault $
 *
 * Project:  GDAL Vector spatial data MongoDB Driver
 * Purpose:  Defines GeoJSON reader within MongoDB Driver.
 * Author:   shuai zhang, zhangshuai.nju@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2014, shuai zhang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#ifndef OGR_MONGOREADER_H_INCLUDED
#define OGR_MONGOREADER_H_INCLUDED

#include <ogr_core.h>
#include <jsonc/json.h> // JSON-C
#include "ogrgeojsonreader.h"
/************************************************************************/
/*                         FORWARD DECLARATIONS                         */
/************************************************************************/

class OGRGeometry;
class OGRRawPoint;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;
class OGRFeature;
class OGRMongoLayer;
class OGRSpatialReference;

/************************************************************************/
/*                           OGRMongoReader                           */
/************************************************************************/

class OGRMongoDataSource;

class OGRMongoReader
{
public:

    OGRMongoReader();
    ~OGRMongoReader();

    void SetPreserveGeometryType( bool bPreserve );
    void SetSkipAttributes( bool bSkip );

    
    OGRMongoLayer* ReadLayer( const char* pszCollName, OGRMongoDataSource* poDS );

private:

    json_object* poGJObject_;
    OGRMongoLayer* poLayer_;
    bool bGeometryPreserve_;
    bool bAttributesSkip_;

    int bFlattenGeocouchSpatiallistFormat;
    bool bFoundId, bFoundRev, bFoundTypeFeature, bIsGeocouchSpatiallistFormat;

    //
    // Copy operations not supported.
    //
    OGRMongoReader( OGRMongoReader const& );
    OGRMongoReader& operator=( OGRMongoReader const& );

    //
    // Translation utilities.
    //
	json_object*  Parse2Jobj( const char* pszText );
    bool GenerateLayerDefn(json_object* gjo);
    bool GenerateFeatureDefn( json_object* poObj );
    bool AddFeature( OGRGeometry* poGeometry );
    bool AddFeature( OGRFeature* poFeature );

    OGRGeometry* ReadGeometry( json_object* poObj );
    OGRFeature* ReadFeature( json_object* poObj );
    OGRMongoLayer* ReadFeatureCollection( json_object* poObj );
};

#endif /* OGR_MONGOREADER_H_INCLUDED */
