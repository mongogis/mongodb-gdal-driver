/******************************************************************************
 * $Id: OGRMongoReader.cpp 23654 2013-12-29 16:19:38Z rouault $
 *
 * Project:  GDAL Vector spatial data MongoDB Driver
 * Purpose:  Implementation of OGRMongoReader class.
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
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"
#include <jsonc/json.h> // JSON-C
#include <jsonc/json_object_private.h> // json_object_iter, complete type required
#include <ogr_api.h>
#include "ogrmongoreader.h"
#include "ogr_mongo.h"
#include "mongo/client/dbclient.h"
using namespace mongo;
/************************************************************************/
/*                           OGRMongoReader                           */
/************************************************************************/

OGRMongoReader::OGRMongoReader()
    : poGJObject_( NULL ), poLayer_( NULL ),
        bGeometryPreserve_( true ),
        bAttributesSkip_( false ),
        bFlattenGeocouchSpatiallistFormat (-1), bFoundId (false), bFoundRev(false), bFoundTypeFeature(false), bIsGeocouchSpatiallistFormat(false)
{
    // Take a deep breath and get to work.
}

/************************************************************************/
/*                          ~OGRMongoReader                           */
/************************************************************************/

OGRMongoReader::~OGRMongoReader()
{
    if( NULL != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = NULL;
    poLayer_ = NULL;
}

/************************************************************************/
/*                           Parse                                      */
/************************************************************************/

json_object*  OGRMongoReader::Parse2Jobj( const char* pszGeoJstring )
{
    if( NULL != pszGeoJstring )
    {
		//!!!!!!
		//TODO try to get the CRS and geometry type of this collection
		//!!!!!

		json_tokener* jstok = NULL;
		json_object* jsobj = NULL;

		jstok = json_tokener_new();
		jsobj = json_tokener_parse_ex(jstok, pszGeoJstring, -1);
		if( jstok->err != json_tokener_success)
		{
			CPLError( CE_Failure, CPLE_AppDefined,
				"GeoJSON parsing error: %s (at offset %d)",
				json_tokener_errors[jstok->err], jstok->char_offset);

			json_tokener_free(jstok);
			return NULL;
		}
		json_tokener_free(jstok);

		/* JSON tree is shared for while lifetime of the reader object
			* and will be released in the destructor.
			*/
			return jsobj;
    }

    return NULL;
}

/************************************************************************/
/*                           ReadLayer                                  */
/************************************************************************/

OGRMongoLayer* OGRMongoReader::ReadLayer( const char* pszCollName, OGRMongoDataSource* poDS )
{
    CPLAssert( NULL == poLayer_ );
	CPLAssert( NULL != poDS );

    poLayer_ = new OGRMongoLayer( pszCollName, NULL,
                                   OGRMongoLayer::DefaultGeometryType,
                                   NULL, poDS );

	mongo::DBClientConnection Conn;
	std::string	errmsg;
	Conn.connect( poDS->GetMongoConnString().c_str() , errmsg ) ;

	if (Conn.isFailed())
	{
		CPLError( CE_Failure, CPLE_NoWriteAccess, 
			"Failed to read MongoLayer: %s. Connection failed!", 
			pszCollName );
		return NULL;
	} 
	string strDBname(poDS->GetName()),strLayer(pszCollName);
	auto_ptr<DBClientCursor> cursor = Conn.query( strDBname+"."+strLayer , BSONObj() );

	const char*  pszGeoJstring;
	json_object* _GeoJSONObject;
	int bfirst=1;
	while ( cursor->more() ) 
	{
		//* -------------------------------------------------------------------- */
		//*      Translate every doc to json object.                  */
		//* -------------------------------------------------------------------- */
		BSONObj obj2 = cursor->next().removeField("_id");
		const char*  pszGeoJstring = CPLStrdup(obj2.jsonString().c_str() );

		if ( !GeoJSONIsObject( pszGeoJstring) )
		 {
			 CPLDebug( "MongoDB", "Not a valid GeoJSON data,ignored...");
			 continue;
		 }

		_GeoJSONObject=Parse2Jobj( pszGeoJstring );

		//* -------------------------------------------------------------------- */
		//*      Translate every doc to json object.                  */
		//* -------------------------------------------------------------------- */
		if (bfirst)
		{
			if( !GenerateLayerDefn(_GeoJSONObject) )
			{
				CPLError( CE_Failure, CPLE_AppDefined,
					"Layer schema generation failed." );

				delete poLayer_;
				return NULL;
			}
			bfirst=0;
		}

	//* -------------------------------------------------------------------- */
	//*      Translate single geometry-only Feature object.                  */
	//* -------------------------------------------------------------------- */
		GeoJSONObject::Type objType = OGRGeoJSONGetType( _GeoJSONObject );

		if( GeoJSONObject::ePoint == objType
			|| GeoJSONObject::eMultiPoint == objType
			|| GeoJSONObject::eLineString == objType
			|| GeoJSONObject::eMultiLineString == objType
			|| GeoJSONObject::ePolygon == objType
			|| GeoJSONObject::eMultiPolygon == objType
			|| GeoJSONObject::eGeometryCollection == objType )
		{
			OGRGeometry* poGeometry = NULL;
			poGeometry = ReadGeometry( _GeoJSONObject );
			if( !AddFeature( poGeometry ) )
			{
				CPLDebug( "GeoJSON",
						  "Translation of single geometry failed." );
				delete poLayer_;
				return NULL;
			}
		}
	//* -------------------------------------------------------------------- */
	//*      Translate single but complete Feature object.                   */
	//* -------------------------------------------------------------------- */
		else if( GeoJSONObject::eFeature == objType )
		{
			OGRFeature* poFeature = NULL;
			poFeature = ReadFeature( _GeoJSONObject );
			if( !AddFeature( poFeature ) )
			{
				CPLDebug( "GeoJSON",
						  "Translation of single feature failed." );

				delete poLayer_;
				return NULL;
			}
		}
	//* -------------------------------------------------------------------- */
	//*      Translate multi-feature FeatureCollection object.               */
	//* -------------------------------------------------------------------- */
		else if( GeoJSONObject::eFeatureCollection == objType )
		{
			OGRMongoLayer* poThisLayer = NULL;
			poThisLayer = ReadFeatureCollection( _GeoJSONObject );
			CPLAssert( poLayer_ == poThisLayer );
		}
		else
		{
			CPLError( CE_Failure, CPLE_AppDefined,
				"Unrecognized GeoJSON structure." );

			delete poLayer_;
			return NULL;
		}
	}

	if (poLayer_->GetFeatureCount()<1)
	{
		delete poLayer_;
		return NULL;
	}

	poLayer_->DetectGeometryType();

	if (poLayer_->GetSpatialRef()==NULL)
	{    
		
		if (Conn.isFailed())
		{
			CPLError( CE_Failure, CPLE_NoWriteAccess, 
				"Failed to get the spatial reference of  %s from mongodb server. Connection failed!", 
				pszCollName );
			return poLayer_;
		} 

		string pname(pszCollName);
		string qstr="{\"name\":\"" + pname+"\"}";
		
		auto_ptr<DBClientCursor> cursor = Conn.query(strDBname+".ogrmetadata",mongo::Query(qstr));
		char* crs_str;

		if ( cursor->more() ) 
		{
			BSONElement lastId = minKey.firstElement();
			BSONObj obj2 = cursor->next();
			lastId =obj2["crs"];
			crs_str=CPLStrdup(lastId.valuestrsafe()) ;
		}

		OGRSpatialReference* poSRS = NULL;
		poSRS = new OGRSpatialReference();

		//poSRS = OGRGeoJSONReadSpatialReference( _GeoJSONObject );
		if (OGRERR_NONE == poSRS->importFromWkt(&crs_str)) 
		{
			poLayer_->SetSpatialRef( poSRS );
			poSRS->~OGRSpatialReference();
			poSRS = NULL;
		}
	}
	
    return poLayer_;
}


/************************************************************************/
/*                           SetPreserveGeometryType                    */
/************************************************************************/

void OGRMongoReader::SetPreserveGeometryType( bool bPreserve )
{
    bGeometryPreserve_ = bPreserve;
}

/************************************************************************/
/*                           SetSkipAttributes                          */
/************************************************************************/

void OGRMongoReader::SetSkipAttributes( bool bSkip )
{
    bAttributesSkip_ = bSkip;
}

/************************************************************************/
/*                           GenerateFeatureDefn                        */
/************************************************************************/

bool OGRMongoReader::GenerateLayerDefn(json_object* GjObject)
{
    CPLAssert( NULL != GjObject );
    CPLAssert( NULL != poLayer_->GetLayerDefn() );
    CPLAssert( 0 == poLayer_->GetLayerDefn()->GetFieldCount() );

    bool bSuccess = true;

    if( bAttributesSkip_ )
        return true;

/* -------------------------------------------------------------------- */
/*      Scan all features and generate layer definition.				*/
/* -------------------------------------------------------------------- */
    GeoJSONObject::Type objType = OGRGeoJSONGetType( GjObject );
    if( GeoJSONObject::eFeature == objType )
    {
        bSuccess = GenerateFeatureDefn( GjObject );
    }
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        json_object* poObjFeatures = NULL;
        poObjFeatures = OGRGeoJSONFindMemberByName( GjObject, "features" );
        if( NULL != poObjFeatures
            && json_type_array == json_object_get_type( poObjFeatures ) )
        {
            json_object* poObjFeature = NULL;
            const int nFeatures = json_object_array_length( poObjFeatures );
            for( int i = 0; i < nFeatures; ++i )
            {
                poObjFeature = json_object_array_get_idx( poObjFeatures, i );
                if( !GenerateFeatureDefn( poObjFeature ) )
                {
                    CPLDebug( "GeoJSON", "Create feature schema failure." );
                    bSuccess = false;
                }
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid FeatureCollection object. "
                      "Missing \'features\' member." );
            bSuccess = false;
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate and add FID column if necessary.                       */
/* -------------------------------------------------------------------- */
	OGRFeatureDefn* poLayerDefn = poLayer_->GetLayerDefn();
	CPLAssert( NULL != poLayerDefn );

	bool bHasFID = false;

	for( int i = 0; i < poLayerDefn->GetFieldCount(); ++i )
	{
		OGRFieldDefn* poDefn = poLayerDefn->GetFieldDefn(i);
		if( EQUAL( poDefn->GetNameRef(), OGRMongoLayer::DefaultFIDColumn )
			&& OFTInteger == poDefn->GetType() )
		{
			poLayer_->SetFIDColumn( poDefn->GetNameRef() );
            bHasFID = true;
            break;
		}
	}

    // TODO - mloskot: This is wrong! We want to add only FID field if
    // found in source layer (by default name or by FID_PROPERTY= specifier,
    // the latter has to be implemented).
    /*
    if( !bHasFID )
    {
        OGRFieldDefn fldDefn( OGRMongoLayer::DefaultFIDColumn, OFTInteger );
        poLayerDefn->AddFieldDefn( &fldDefn );
        poLayer_->SetFIDColumn( fldDefn.GetNameRef() );
    }
    */

    return bSuccess;
}

bool OGRMongoReader::GenerateFeatureDefn( json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
    CPLAssert( NULL != poDefn );

    bool bSuccess = false;

/* -------------------------------------------------------------------- */
/*      Read collection of properties.									*/
/* -------------------------------------------------------------------- */
    json_object* poObjProps = NULL;
    poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            poObjProps = json_object_object_get(poObjProps, "properties");
            if( NULL == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object )
            {
                return true;
            }
        }

        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            int nFldIndex = poDefn->GetFieldIndex( it.key );
            if( -1 == nFldIndex )
            {
                /* Detect the special kind of GeoJSON output by a spatiallist of GeoCouch */
                /* such as http://gd.iriscouch.com/cphosm/_design/geo/_rewrite/data?bbox=12.53%2C55.73%2C12.54%2C55.73 */
                if (strcmp(it.key, "_id") == 0)
                    bFoundId = true;
                else if (bFoundId && strcmp(it.key, "_rev") == 0)
                    bFoundRev = true;
                else if (bFoundRev && strcmp(it.key, "type") == 0 &&
                         it.val != NULL && json_object_get_type(it.val) == json_type_string &&
                         strcmp(json_object_get_string(it.val), "Feature") == 0)
                    bFoundTypeFeature = true;
                else if (bFoundTypeFeature && strcmp(it.key, "properties") == 0 &&
                         it.val != NULL && json_object_get_type(it.val) == json_type_object)
                {
                    if (bFlattenGeocouchSpatiallistFormat < 0)
                        bFlattenGeocouchSpatiallistFormat = CSLTestBoolean(
                            CPLGetConfigOption("GEOJSON_FLATTEN_GEOCOUCH", "TRUE"));
                    if (bFlattenGeocouchSpatiallistFormat)
                    {
                        poDefn->DeleteFieldDefn(poDefn->GetFieldIndex("type"));
                        bIsGeocouchSpatiallistFormat = true;
                        return GenerateFeatureDefn(poObj);
                    }
                }

                OGRFieldDefn fldDefn( it.key,
                    GeoJSONPropertyToFieldType( it.val ) );
                poDefn->AddFieldDefn( &fldDefn );
            }
            else
            {
                OGRFieldDefn* poFDefn = poDefn->GetFieldDefn(nFldIndex);
                OGRFieldType eType = poFDefn->GetType();
                if( eType == OFTInteger )
                {
                    OGRFieldType eNewType = GeoJSONPropertyToFieldType( it.val );
                    if( eNewType == OFTReal )
                        poFDefn->SetType(eNewType);
                }
            }
        }

        bSuccess = true; // SUCCESS
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'properties\' member." );
    }

    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRMongoReader::AddFeature( OGRGeometry* poGeometry )
{
    bool bAdded = false;

    // TODO: Should we check if geometry is of type of 
    //       wkbGeometryCollection ?

    if( NULL != poGeometry )
    {
        OGRFeature* poFeature = NULL;
        poFeature = new OGRFeature( poLayer_->GetLayerDefn() );
        poFeature->SetGeometryDirectly( poGeometry );

        bAdded = AddFeature( poFeature );
    }
    
    return bAdded;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRMongoReader::AddFeature( OGRFeature* poFeature )
{
    bool bAdded = false;
  
    if( NULL != poFeature )
    {
        poLayer_->AddFeature( poFeature );
        bAdded = true;
        delete poFeature;
    }

    return bAdded;
}

/************************************************************************/
/*                           ReadGeometry                               */
/************************************************************************/

OGRGeometry* OGRMongoReader::ReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;

    poGeometry = OGRGeoJSONReadGeometry( poObj );

/* -------------------------------------------------------------------- */
/*      Wrap geometry with GeometryCollection as a common denominator.  */
/*      Sometimes a GeoJSON text may consist of objects of different    */
/*      geometry types. Users may request wrapping all geometries with  */
/*      OGRGeometryCollection type by using option                      */
/*      GEOMETRY_AS_COLLECTION=NO|YES (NO is default).                 */
/* -------------------------------------------------------------------- */
    if( NULL != poGeometry )
    {
        if( !bGeometryPreserve_ 
            && wkbGeometryCollection != poGeometry->getGeometryType() )
        {
            OGRGeometryCollection* poMetaGeometry = NULL;
            poMetaGeometry = new OGRGeometryCollection();
            poMetaGeometry->addGeometryDirectly( poGeometry );
            return poMetaGeometry;
        }
    }

    return poGeometry;
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRMongoReader::ReadFeature( json_object* poObj )
{
    CPLAssert( NULL != poObj );
    CPLAssert( NULL != poLayer_ );

    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( poLayer_->GetLayerDefn() );

/* -------------------------------------------------------------------- */
/*      Translate GeoJSON "properties" object to feature attributes.    */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = NULL;
    poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( !bAttributesSkip_ && NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            json_object* poId = json_object_object_get(poObjProps, "_id");
            if (poId != NULL && json_object_get_type(poId) == json_type_string)
                poFeature->SetField( "_id", json_object_get_string(poId) );

            json_object* poRev = json_object_object_get(poObjProps, "_rev");
            if (poRev != NULL && json_object_get_type(poRev) == json_type_string)
                poFeature->SetField( "_rev", json_object_get_string(poRev) );

            poObjProps = json_object_object_get(poObjProps, "properties");
            if( NULL == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object )
            {
                return poFeature;
            }
        }

        int nField = -1;
        OGRFieldDefn* poFieldDefn = NULL;
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            nField = poFeature->GetFieldIndex(it.key);
            poFieldDefn = poFeature->GetFieldDefnRef(nField);
            CPLAssert( NULL != poFieldDefn );
            OGRFieldType eType = poFieldDefn->GetType();

            if( it.val == NULL)
            {
                /* nothing to do */
            }
            else if( OFTInteger == eType )
			{
                poFeature->SetField( nField, json_object_get_int(it.val) );
				
				/* Check if FID available and set correct value. */
				if( EQUAL( it.key, poLayer_->GetFIDColumn() ) )
					poFeature->SetFID( json_object_get_int(it.val) );
			}
            else if( OFTReal == eType )
			{
                poFeature->SetField( nField, json_object_get_double(it.val) );
			}
            else if( OFTIntegerList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    int* panVal = (int*)CPLMalloc(sizeof(int) * nLength);
                    for(int i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
                        panVal[i] = json_object_get_int(poRow);
                    }
                    poFeature->SetField( nField, nLength, panVal );
                    CPLFree(panVal);
                }
            }
            else if( OFTRealList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    double* padfVal = (double*)CPLMalloc(sizeof(double) * nLength);
                    for(int i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
                        padfVal[i] = json_object_get_double(poRow);
                    }
                    poFeature->SetField( nField, nLength, padfVal );
                    CPLFree(padfVal);
                }
            }
            else if( OFTStringList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    char** papszVal = (char**)CPLMalloc(sizeof(char*) * (nLength+1));
                    int i;
                    for(i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
                        const char* pszVal = json_object_get_string(poRow);
                        if (pszVal == NULL)
                            break;
                        papszVal[i] = CPLStrdup(pszVal);
                    }
                    papszVal[i] = NULL;
                    poFeature->SetField( nField, papszVal );
                    CSLDestroy(papszVal);
                }
            }
            else
			{
                poFeature->SetField( nField, json_object_get_string(it.val) );
			}
        }
    }

/* -------------------------------------------------------------------- */
/*      If FID not set, try to use feature-level ID if available        */
/*      and of integral type. Otherwise, leave unset (-1) then index    */
/*      in features sequence will be used as FID.                       */
/* -------------------------------------------------------------------- */
    if( -1 == poFeature->GetFID() )
    {
        json_object* poObjId = NULL;
        poObjId = OGRGeoJSONFindMemberByName( poObj, OGRMongoLayer::DefaultFIDColumn );
        if( NULL != poObjId
            && EQUAL( OGRMongoLayer::DefaultFIDColumn, poLayer_->GetFIDColumn() )
            && OFTInteger == GeoJSONPropertyToFieldType( poObjId ) )
        {
            poFeature->SetFID( json_object_get_int( poObjId ) );
            int nField = poFeature->GetFieldIndex( poLayer_->GetFIDColumn() );
            if( -1 != nField )
                poFeature->SetField( nField, (int) poFeature->GetFID() );
        }
    }

    if( -1 == poFeature->GetFID() )
    {
        json_object* poObjId = OGRGeoJSONFindMemberByName( poObj, "id" );
        if (poObjId != NULL && json_object_get_type(poObjId) == json_type_int)
            poFeature->SetFID( json_object_get_int( poObjId ) );
    }

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of GeoJSON Feature.               */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = NULL;

    json_object* poTmp = poObj;

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;    
    json_object_object_foreachC(poTmp, it)
    {
        if( EQUAL( it.key, "geometry" ) ) {
            if (it.val != NULL)
                poObjGeom = it.val;
            // we're done.  They had 'geometry':null
            else
                return poFeature;
        }
    }
    
    if( NULL != poObjGeom )
    {
        // NOTE: If geometry can not be parsed or read correctly
        //       then NULL geometry is assigned to a feature and
        //       geometry type for layer is classified as wkbUnknown.
        OGRGeometry* poGeometry = ReadGeometry( poObjGeom );
        if( NULL != poGeometry )
        {
            poFeature->SetGeometryDirectly( poGeometry );
			//poFeature->SetGeometry(poGeometry);
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'geometry\' member." );
        delete poFeature;
        return NULL;
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

OGRMongoLayer*
OGRMongoReader::ReadFeatureCollection( json_object* poObj )
{
    CPLAssert( NULL != poLayer_ );

    json_object* poObjFeatures = NULL;
    poObjFeatures = OGRGeoJSONFindMemberByName( poObj, "features" );
    if( NULL == poObjFeatures )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid FeatureCollection object. "
                  "Missing \'features\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjFeatures ) )
    {
        bool bAdded = false;
        OGRFeature* poFeature = NULL;
        json_object* poObjFeature = NULL;

        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            poObjFeature = json_object_array_get_idx( poObjFeatures, i );
            poFeature = OGRMongoReader::ReadFeature( poObjFeature );
            bAdded = AddFeature( poFeature );
            //CPLAssert( bAdded );
        }
        //CPLAssert( nFeatures == poLayer_->GetFeatureCount() );
    }

    // We're returning class member to follow the same pattern of
    // Read* functions call convention.
    CPLAssert( NULL != poLayer_ );
    return poLayer_;
}
