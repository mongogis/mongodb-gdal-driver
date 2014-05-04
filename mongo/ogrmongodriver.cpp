/******************************************************************************
 * $Id: OGRMongoDriver.cpp 19489 2013-07-08 15:09:05Z rouault $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRMongoDriver class (OGR MongoDB Driver).
 * Author:   Shuai Zhang, zhangshuai.nju@gmail.com
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
#include <cpl_conv.h>

/************************************************************************/
/*                           OGRMongoDriver()                         */
/************************************************************************/

OGRMongoDriver::OGRMongoDriver()
{
}

/************************************************************************/
/*                          ~OGRMongoDriver()                         */
/************************************************************************/

OGRMongoDriver::~OGRMongoDriver()
{
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* OGRMongoDriver::GetName()
{
    return "MongoDB";
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

OGRDataSource* OGRMongoDriver::Open( const char* pszName, int bUpdate )
{
    return Open( pszName, bUpdate, NULL );
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

OGRDataSource* OGRMongoDriver::Open( const char* pszName, int bUpdate,
                                       char** papszOptions )
{
    UNREFERENCED_PARAM(papszOptions);

    OGRMongoDataSource* poDS = NULL;
    poDS = new OGRMongoDataSource();

/* -------------------------------------------------------------------- */
/*      Processing configuration options.                               */
/* -------------------------------------------------------------------- */

    // TODO: Currently, options are based on environment variables.
    //       This is workaround for not yet implemented Andrey's concept
    //       described in document 'RFC 10: OGR Open Parameters'.

    poDS->SetGeometryTranslation( OGRMongoDataSource::eGeometryPreserve );
    const char* pszOpt = CPLGetConfigOption("GEOMETRY_AS_COLLECTION", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
            poDS->SetGeometryTranslation(
                OGRMongoDataSource::eGeometryAsCollection );
    }

    poDS->SetAttributesTranslation( OGRMongoDataSource::eAtributesPreserve );
    pszOpt = CPLGetConfigOption("ATTRIBUTES_SKIP", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
        poDS->SetAttributesTranslation( 
            OGRMongoDataSource::eAtributesSkip );
    }

/* -------------------------------------------------------------------- */
/*      Open and start processing MongoDB datasoruce to OGR objects.    */
/* -------------------------------------------------------------------- */
    if( !poDS->Open( pszName ) )
    {
        //delete poDS;
		poDS->~OGRMongoDataSource();
        poDS= NULL;
    }

    if( NULL != poDS && bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "MongoDB Driver doesn't support update." );
        poDS->~OGRMongoDataSource();
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           CreateDataSource()                         */
/************************************************************************/

OGRDataSource* OGRMongoDriver::CreateDataSource( const char* pszName,
                                                   char** papszOptions )
{
    OGRMongoDataSource* poDS = new OGRMongoDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           DeleteDataSource()                         */
/************************************************************************/

OGRErr OGRMongoDriver::DeleteDataSource( const char* pszName )
{
	OGRMongoDataSource* poDS = new OGRMongoDataSource();
	OGRErr tOgrErr =poDS->Delete( pszName );
	delete poDS;
	poDS = NULL;
	return tOgrErr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMongoDriver::TestCapability( const char* pszCap )
{
    if( EQUAL( pszCap, ODrCCreateDataSource ) )
        return TRUE;
    else if( EQUAL(pszCap, ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRMongo()                       */
/************************************************************************/

void RegisterOGRMONGO()
{
	if( GDAL_CHECK_VERSION("OGR/MONGO driver") )
	{
		OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRMongoDriver );
	}
}
#endif