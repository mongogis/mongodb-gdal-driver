/******************************************************************************
 * $Id: ogr_Mongo.h 23367 2013-5-12 22:46:13Z rouault $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions of OGR OGR_Mongo driver types.
 * Author:   Zhang Shuai, from Nanjing University, China
 * Email:	 zhangshuai.nju@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Zhang Shuai
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
#define OGR_Mongo_H_INCLUDED
#ifdef OGR_Mongo_H_INCLUDED
//

#include <ogrsf_frmts.h>
#include <cstdio>
#include <vector> // used by OGRMongoLayer
#include <cpl_http.h>
#include "jsonc/json.h" // JSON-C
#include <cstddef>
#include <cstdlib>
#include "mongo/client/dbclient.h"
#include <ogr_core.h>
#include "ogr_geojson.h"

#define SPACE_FOR_BBOX  80
#define Geo_COLL_inDB "_ogr_metadata"

class OGRMongoDataSource;
using namespace std;
/************************************************************************/
/*                           OGRMongoLayer                            */
/************************************************************************/

class OGRMongoLayer : public OGRLayer
{
public:
	

    static const char* const DefaultName;
    static const char* const DefaultFIDColumn;
    static const OGRwkbGeometryType DefaultGeometryType;
 
    OGRMongoLayer( const char* pszName,
                     OGRSpatialReference* poSRS,
                     OGRwkbGeometryType eGType,
                     char** papszOptions,
                     OGRMongoDataSource* poDS );
    ~OGRMongoLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn* GetLayerDefn();
    OGRSpatialReference* GetSpatialRef();
    
    int GetFeatureCount( int bForce = TRUE );
    void ResetReading();
    OGRFeature* GetNextFeature();
    OGRFeature* GetFeature( long nFID );
    OGRErr CreateFeature( OGRFeature* poFeature );
    OGRErr CreateField(OGRFieldDefn* poField, int bApproxOK);
    int TestCapability( const char* pszCap );
    const char* GetFIDColumn();
    void SetFIDColumn( const char* pszFIDColumn );
    
    //
    // OGRMongoLayer Interface
    //
    void AddFeature( OGRFeature* poFeature );
    void SetSpatialRef( OGRSpatialReference* poSRS );
    void DetectGeometryType();
private:
	
    typedef std::vector<OGRFeature*> FeaturesSeq;
    FeaturesSeq seqFeatures_;
    FeaturesSeq::iterator iterCurrent_;

    OGRMongoDataSource* poDS_;
    OGRFeatureDefn* poFeatureDefn_;
    OGRSpatialReference* poSRS_;
    CPLString sFIDColumn_;
    int nOutCounter_;

    int bWriteBBOX;
    int bBBOX3D;
    OGREnvelope3D sEnvelopeLayer;

    int nCoordPrecision;
};

/************************************************************************/
/*                           OGRMongoDataSource                       */
/************************************************************************/

class OGRMongoDataSource : public OGRDataSource
{
public:

    OGRMongoDataSource();
    ~OGRMongoDataSource();

    //
    // OGRDataSource Interface
    //
    int Open( const char* pszSource );
    const char* GetName();
    int GetLayerCount();
    OGRLayer* GetLayer( int nLayer );
    OGRLayer* CreateLayer( const char* pszName,
                           OGRSpatialReference* poSRS = NULL,
                           OGRwkbGeometryType eGType = wkbUnknown,
                           char** papszOptions = NULL );
    int TestCapability( const char* pszCap );

    //
    // OGRMongoDataSource Interface
    //
    int Create( const char* pszName, char** papszOptions );
	int Delete(const char* pszName);

    enum GeometryTranslation
    {
        eGeometryPreserve,
        eGeometryAsCollection,
    };
    
    void SetGeometryTranslation( GeometryTranslation type );

    enum AttributesTranslation
    {
        eAtributesPreserve,
        eAtributesSkip
    };

    void SetAttributesTranslation( AttributesTranslation type );

    int  GetFpOutputIsSeekable() const { return bFpOutputIsSeekable_; }
    int  GetBBOXInsertLocation() const { return nBBOXInsertLocation_; }
	
	//mongo::DBClientConnection * MongoConn();
	string  GetMongoConnString();
	string  GetCollName();
	string  GetHost();
	string  GetPort();
private:

    //
    // Private data members
    //

    OGRMongoLayer** papoLayers_;
    int nLayers_;
	string  pszConnectionString_;
	string  _oHost, _nPort, _oPassword, _oUser, _oDB, _oCollection;
    
    //
    // Translation/Creation control flags
    // 
    GeometryTranslation flTransGeom_;
    AttributesTranslation flTransAttrs_;

    int bFpOutputIsSeekable_;
    int nBBOXInsertLocation_;

    //
    // Priavte utility functions
    //
    void Clear();
	bool TestConnection(const char* mongoConnstring);
	//mongo::DBClientConnection * _conn;
    OGRMongoLayer* LoadLayer(const char* pLayerName);
};


/************************************************************************/
/*                           OGRMongoDriver                           */
/************************************************************************/

class OGRMongoDriver : public OGRSFDriver
{
public:

    OGRMongoDriver();
    ~OGRMongoDriver();

    //
    // OGRSFDriver Interface
    //
    const char* GetName();
    OGRDataSource* Open( const char* pszName, int bUpdate );
    OGRDataSource* CreateDataSource( const char* pszName, char** papszOptions );
    OGRErr DeleteDataSource( const char* pszName );
    int TestCapability( const char* pszCap );

    //
    // OGRMongoDriver Interface
    //
    // NOTE: New version of Open() based on Andrey's RFC 10.
    OGRDataSource* Open( const char* pszName, int bUpdate,
                         char** papszOptions );

};

#endif /* OGR_Mongo_H_INCLUDED */

