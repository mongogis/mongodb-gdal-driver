/******************************************************************************
* $Id: OGRMongoDataSource.cpp 23367 2011-11-12 22:46:13Z rouault $
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Implementation of OGRMongoDataSource class (OGR GeoJSON Driver).
* Author:   Mateusz Loskot, mateusz@loskot.net
*
******************************************************************************
* Copyright (c) 2007, Mateusz Loskot
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
#include "ogr_mongo.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsonreader.h"
#include <cpl_http.h>
#include "jsonc/json.h" // JSON-C
#include <cstddef>
#include <cstdlib>
#include "mongo/client/dbclient.h"
#include "ogrmongoreader.h"
using namespace std;
using namespace mongo;
/************************************************************************/
/*                           OGRMongoDataSource()                     */
/************************************************************************/

OGRMongoDataSource::OGRMongoDataSource()
	:papoLayers_(NULL), nLayers_(0), 
	flTransGeom_( OGRMongoDataSource::eGeometryPreserve ),
	flTransAttrs_( OGRMongoDataSource::eAtributesPreserve ),
	bFpOutputIsSeekable_( FALSE ),
	nBBOXInsertLocation_(0)
{
	// I've got constructed. Lunch time!
	//OGRGeoJSONDataSource();
	_oHost="127.0.0.1";
	_nPort = "27017";
	_oPassword="";
	_oUser="";
	_oDB="test";
	_oCollection="foo";
	pszConnectionString_="127.0.0.1:27017";
	//_conn=NULL;
}

/************************************************************************/
/*                           ~OGRMongoDataSource()                    */
/************************************************************************/

OGRMongoDataSource::~OGRMongoDataSource()
{
	Clear();
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

int OGRMongoDataSource::Open( const char* pszConnectionString )
{
	CPLAssert( NULL != pszConnectionString );

	if (!EQUALN(pszConnectionString,"MONGO:",6))
	{
		Clear();
		return FALSE;
	}
	if( !TestConnection( pszConnectionString ) )
	{
		CPLError( CE_Failure, CPLE_AppDefined, 
			"%s does not conform to MongoDB naming convention,"
			" mongo:[host=127.0.0.1][,port=27017][,db=test][,collection=foo][,user=...][,pwd=...]",
			pszConnectionString );
		Clear();
		return FALSE;
	}
	
	/* -------------------------------------------------------------------- */
	/*      Release resources allocated during previous request.            */
	/* -------------------------------------------------------------------- */
	if( NULL != papoLayers_ )
	{
		CPLAssert( nLayers_ > 0 );
		Clear();
	}

	OGRMongoLayer* poLayer = LoadLayer(_oCollection.c_str());

	if( NULL == poLayer )
	{
		Clear();

		CPLError( CE_Failure, CPLE_OpenFailed, 
			"Failed to read Geo-data from the mongodb server." );
		return FALSE;
	}

	/////* -------------------------------------------------------------------- */
	/////*      NOTE: Currently, the driver generates only one layer per        */
	/////*      single GeoJSON file, input or service request.                  */
	/////* -------------------------------------------------------------------- */
	const int nLayerIndex = 0;
	nLayers_ = 1;

	papoLayers_ =
		(OGRMongoLayer**)CPLMalloc( sizeof(OGRMongoLayer*) * nLayers_ );
	papoLayers_[nLayerIndex] = poLayer; 

	CPLAssert( NULL != papoLayers_ );
	CPLAssert( nLayers_ > 0 );
	return TRUE;
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* OGRMongoDataSource::GetName()
{
	return !_oDB.empty() ? _oDB.c_str() : "";
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRMongoDataSource::GetLayerCount()
{
	return nLayers_;
}

string  OGRMongoDataSource::GetCollName()
{
	return _oCollection;
}

string OGRMongoDataSource::GetHost()
{
	return _oHost;
}

string OGRMongoDataSource::GetMongoConnString()
{
	return pszConnectionString_;
}

string OGRMongoDataSource::GetPort()
{
	return _nPort;
}
//
//mongo::DBClientConnection * OGRMongoDataSource::MongoConn()
//{
//	return _conn;
//	//return NULL;
//}
/************************************************************************/
/*                           GetLayer()                                 */
/************************************************************************/

OGRLayer* OGRMongoDataSource::GetLayer( int nLayer )
{
	if( 0 <= nLayer || nLayer < nLayers_ )
	{
		CPLAssert( NULL != papoLayers_[nLayer] );

		OGRLayer* poLayer = papoLayers_[nLayer];

		/* Return layer in readable state. */
		poLayer->ResetReading();
		return poLayer;
	}

	return NULL;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer* OGRMongoDataSource::CreateLayer( const char* pszName_,
	OGRSpatialReference* poSRS,
	OGRwkbGeometryType eGType,
	char** papszOptions )
{
	OGRMongoLayer* poLayer = NULL;
	std::string errmsg;
	mongo::DBClientConnection Conn;
	Conn.connect( pszConnectionString_ , errmsg ) ;

	if (Conn.isFailed())
	{
		CPLError( CE_Failure, CPLE_NoWriteAccess, 
			"Failed to create mongolayer: %s. Connection failed!", 
			pszName_ );
		return NULL;
	}
	string pname(pszName_),dbnamestr(_oDB);
	string qstr="{\"name\":\"" + pname+"\"}";
	auto_ptr<DBClientCursor> cursor = Conn.query(dbnamestr+".ogrmetadata",mongo::Query(qstr));
	if (cursor->more())
	{
		CPLError( CE_Failure, CPLE_IllegalArg , 
			"Failed to create mongolayer: %s. there already has a layer named %s!", 
			pszName_ ,pszName_);
		cursor->~DBClientCursor();
		return NULL;
	}
	
	poLayer = new OGRMongoLayer( pszName_, poSRS, eGType, papszOptions, this );

	/* -------------------------------------------------------------------- */
	/*      Add layer to data source layer list.                            */
	/* -------------------------------------------------------------------- */

	// TOOD: Waiting for multi-layer support
	if ( nLayers_ != 0 )
	{
		CPLError(CE_Failure, CPLE_NotSupported,
			"GeoJSON driver doesn't support creating more than one layer");
		return NULL;
	}
	
	papoLayers_ = (OGRMongoLayer **)
		CPLRealloc( papoLayers_,  sizeof(OGRMongoLayer*) * (nLayers_ + 1) );

	papoLayers_[nLayers_++] = poLayer;

	char * srswkt="";
	if (NULL!=poSRS)
		poSRS->exportToWkt(&srswkt);

	if (!Conn.isFailed())
	{	
		BSONObj p = BSON( "name" << pszName_<< "crs" << srswkt<<"geotype"<< eGType<<"created"<<DATENOW<<"modified"<<"");
		Conn.createCollection(dbnamestr+"."+pszName_);
		Conn.insert(dbnamestr+".ogrmetadata",p);	
		return poLayer;
	}else{
		CPLError( CE_Failure, CPLE_NoWriteAccess, 
			"Failed to create mongolayer: %s. Connection failed!", 
			pszName_ );
		return NULL;
	}
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMongoDataSource::TestCapability( const char* pszCap )
{
	if( EQUAL( pszCap, ODsCCreateLayer ) )
		return TRUE;
	else if( EQUAL( pszCap, ODsCDeleteLayer ) )
		return TRUE;
	else
		return FALSE;
}
OGRErr OGRMongoDataSource::Delete(const char* pszName)
{
	if (TestConnection(pszName))
	{
		mongo::DBClientConnection Conn;
		std::string	errmsg;
		Conn.connect( pszConnectionString_ , errmsg ) ;

		for each( std::string a in Conn.getDatabaseNames() )
		{	
			if (EQUAL(_oDB.c_str(),a.c_str()))
			{
				mongo::BSONObj *c=new mongo::BSONObj();
				Conn.dropDatabase(a,c);
				CPLDebug( "MongoDB", "%s", c->jsonString());
				//Conn.~DBClientConnection();
				return OGRERR_NONE;
			} 
		}
	} 
	else
	{
		CPLError( CE_Failure, CPLE_OpenFailed, 
			"Failed to get connected by this ConnectionString: %s.", 
			pszName );
		return OGRERR_FAILURE;
	}

	CPLDebug( "mongodb", "Failed to delete \'%s\'", pszName);

	return OGRERR_FAILURE;
}
//////////////////////////////////////////////////////////////////////////
/// if both the database name and the collection name exist,reject!
///
int OGRMongoDataSource::Create( const char* pszName, char** papszOptions )
{
	UNREFERENCED_PARAM(papszOptions);

	CPLAssert( NULL != pszName );
	
	if (!TestConnection(pszName))
	{
		CPLError( CE_Failure, CPLE_OpenFailed, 
			"Failed to get connected by this connectionstring: %s.", 
			pszName );
		return FALSE;
	}
	else{
		std::string errmsg;
		mongo::DBClientConnection Conn;
		Conn.connect( pszConnectionString_ , errmsg ) ;

		for each (string a in Conn.getDatabaseNames())
		{
			if (EQUAL(_oDB.c_str(),a.c_str()))
			{
				for each(string c in Conn.getCollectionNames(_oDB))
					if (EQUAL(_oCollection.c_str(),c.c_str()))
					{				
						CPLError( CE_Failure, CPLE_IllegalArg, 
						"Failed to create datasource for there is already db named: %s.", 
						pszName );
						return FALSE;
					}
			}
		}
	}
	return TRUE;
}

/************************************************************************/
/*                           SetGeometryTranslation()                   */
/************************************************************************/

void
	OGRMongoDataSource::SetGeometryTranslation( GeometryTranslation type )
{
	flTransGeom_ = type;
}

/************************************************************************/
/*                           SetAttributesTranslation()                 */
/************************************************************************/

void OGRMongoDataSource::SetAttributesTranslation( AttributesTranslation type )
{
	flTransAttrs_ = type;
}

/************************************************************************/
/*                  PRIVATE FUNCTIONS IMPLEMENTATION                    */
/************************************************************************/

void OGRMongoDataSource::Clear()
{
	for( int i = 0; i < nLayers_; i++ )
	{
		CPLAssert( NULL != papoLayers_ );
		delete papoLayers_[i];
	}

	CPLFree( papoLayers_ );
	papoLayers_ = NULL;
	nLayers_ = 0;
}

/************************************************************************/
/*                           TestConnection()                           */
/*	(1) to confirm that the string start with "mongo:".					*/
/*	(2) to parse the parameters from the connection string.				*/
/*	(3) to test whether the connection string works.					*/
/*																		*/
/*	Any String that can not pass this test is thought to be invalid.	*/
/************************************************************************/
bool OGRMongoDataSource::TestConnection(const char* pszMongoConnstring)
{
	//**********************************************************************/
	//the mongo connection string will go like this:
	//		mongo:[host=127.0.0.1][,port=27017][,db=test][,collection=foo][,user=...][,pwd=...]
	//
	//**************************************************************************/

	if( NULL == pszMongoConnstring )
	{
		CPLError(CE_Failure, CPLE_OpenFailed, "Input MongoDB connection string is null" );
		return FALSE;
	}
	
	if (!EQUALN(pszMongoConnstring,"MONGO:",6))
		return FALSE;
	
	CPLAssert( nLayers_ == 0 );

	/* -------------------------------------------------------------------- */
	/*      Parse the parameters in the connection string.                  */
	/* -------------------------------------------------------------------- */
	int i;
	char **papszCollNames=NULL;
	char **papszItems = CSLTokenizeString2( pszMongoConnstring+6, ",", CSLT_HONOURSTRINGS );

	if( CSLCount(papszItems) >= 1 )
	{
		for( i = 0; papszItems[i] != NULL; i++ )
		{
			if( EQUALN(papszItems[i],"user=",5) )
				_oUser = papszItems[i] + 5;
			else if( EQUALN(papszItems[i],"pwd=",4) )
				_oPassword = papszItems[i] + 4;
			else if( EQUALN(papszItems[i],"host=",5) )
				_oHost = papszItems[i] + 5;
			else if( EQUALN(papszItems[i],"port=",5) )
				_nPort = papszItems[i] + 5;
			else if( EQUALN(papszItems[i],"db=",3) )
				_oDB = papszItems[i] + 3;
			else if( EQUALN(papszItems[i],"collection=",11) )
			{
				//papszCollNames = CSLTokenizeStringComplex( 
				//	papszItems[i] + 11, ";", FALSE, FALSE );
				_oCollection=papszItems[i] + 11;
			}
			else
				CPLError( CE_Warning, CPLE_AppDefined, 
				"'%s' in MongoDB datasource definition not recognised and ignored.", papszItems[i] );
		}		
	}

	/* -------------------------------------------------------------------- */
	/*      Test whether the connection string works.                  */
	/* -------------------------------------------------------------------- */
	string errmsg;
	int reslt=FALSE;
	try
	{
		mongo::DBClientConnection Conn;
		pszConnectionString_=_oHost+":"+_nPort;
		Conn.connect( pszConnectionString_ , errmsg ) ;

		reslt= TRUE;
	}
	catch (exception e)
	{
		CPLError( CE_Failure, CPLE_OpenFailed, 
			"Can not open the Mongo Datasource using the following Connection string: %s.", pszMongoConnstring );
	}

	CSLDestroy( papszItems );
	return reslt;	
}

/************************************************************************/
/*                           LoadLayer()                          */
/************************************************************************/

OGRMongoLayer* OGRMongoDataSource::LoadLayer(const char* pLayerName)
{
	std::string errmsg;
	mongo::DBClientConnection Conn;
	Conn.connect( pszConnectionString_ , errmsg ) ;

	if (Conn.isFailed())
	{
		CPLError( CE_Failure, CPLE_NoWriteAccess, 
			"Failed to get mongolayer: %s. Connection failed!", 
			pLayerName );
		return NULL;
	}

	string pname(pLayerName),dbnamestr(_oDB),strLayer(pLayerName);
	string qstr="{\"name\":\"" + pname+"\"}";
	auto_ptr<DBClientCursor> cursor = Conn.query(dbnamestr+".ogrmetadata",mongo::Query(qstr));
	if (!cursor->more())
	{
		CPLError( CE_Failure, CPLE_IllegalArg , 
			"Failed to find mongolayer: %s. there isn't a layer named %s! in database %s.", 
			(dbnamestr+"."+strLayer).c_str() ,strLayer.c_str(),dbnamestr.c_str());
		cursor->~DBClientCursor();
		return NULL;
	}

	OGRErr err = OGRERR_NONE;
	OGRMongoLayer* poLayer = NULL;

	/* -------------------------------------------------------------------- */
	/*      Configure GeoJSON format translator.                            */
	/* -------------------------------------------------------------------- */
	OGRMongoReader reader;

	if( eGeometryAsCollection == flTransGeom_ )
	{
		reader.SetPreserveGeometryType( false );
		CPLDebug( "GeoJSON", "Geometry as OGRGeometryCollection type." );
	}

	if( eAtributesSkip == flTransAttrs_ )
	{
		reader.SetSkipAttributes( true );
		CPLDebug( "GeoJSON", "Skip all attributes." );
	}

	/* -------------------------------------------------------------------- */
	/*      Parse GeoJSON and build valid OGRLayer instance.                */
	/* -------------------------------------------------------------------- */
	//err = reader.Parse( pszGeoData_ );
	if( OGRERR_NONE == err )
	{
		// TODO: Think about better name selection
		poLayer = reader.ReadLayer( pLayerName, this );
	}
	//_conn->~DBClientConnection();
	return poLayer;
}
#endif