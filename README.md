# MongoDB OGR Driver for GDAL Libray

We are working to push the mongodb driver into gdal main release. You may find the ticket in the [gdal.org](http://trac.osgeo.org/gdal/ticket/5453).

Right now the GDAL Driver for MongoDB hasn't been built into GDAL official release (we are working on that). The wiki here is determined to guide you compiling it with GDAL source code, so that you won't have to wait. the whole procedure is conducted in Linux OS.

## 1. Preparations
### 1.1 Download Source Codes
Download MongoDB GDAL Driver from [the MongoGIS github](https://github.com/mongogis/mongodb-gdal-driver), [MongoDB source code](http://www.mongodb.org/downloads), [GDAL source code](http://trac.osgeo.org/gdal/wiki/DownloadSource). 

* mongodb-gdal-driver-master.zip
* mongodb-src-r2.4.10.zip
* gdal192.zip
 
Note: after MongoDB version 2.6.0, you could not get CXX driver from its source code. See the [cxx-driver in github](https://github.com/mongodb/mongo-cxx-driver).

### 1.2 Unzip Code Files
Unzip those zip files, get three folders, mongodb-src-r2.6.3, gdal-1.11.0, mongodb-gdal-driver-master.

```bash
unzip *.zip 
```

### 1.3 Compile MongoDB C++ Driver
GDAL Driver will need the include files and lib from MongoDB,so we have to make it ready for use. Go to mongodb-src-r2.6.3 folder, and run, (you have to prepare the [SCons](http://www.scons.org/) to build MongoDB.

```bash
scons --prefix=/opt/mongo install 
```
`--prefix` is for the installation directory, and `--full` enables the “full” installation, directing SCons to install the driver **headers and libraries** to the prefix directory. more info in [github](https://github.com/mongodb/mongo-cxx-driver/wiki/Download%20and%20Compile#build-options) and [mongodb.org](http://www.mongodb.org/about/contributors/tutorial/build-mongodb-from-source/). After the building is done, you will get three folders in the `/opt/mongo`, `bin`, `include`, and `libs`, copy the `boost library` under the mongodb source code folder `src/third_party/boost` into the `include` folder, copy the `*.a` files in the `build/linux2/normal/third_party/boost/` folder into `libs`.

So in the end you got a mongo folder going like this:

* `bin`
* `include/mongo`
* `include/boost`
* `lib/libboost_filesystem.a`, `lib/libboost_program_options.a`, `lib/libboost_system.a`, `lib/libboost_thread.a`, `lib/libmongoclient.a`

# 2. Incorporate mongo into GDAL architecture
## 2.1 Move into ogrsf_frmts folder
Move `mongodb-gdal-driver-master` into `gdal/ogrsf_frmts` folder and rename it as `mongo`.

```bash
mv mongodb-gdal-driver-master/ gdal-1.11.0/ogr/ogrsf_frmts/mongo
```

## 2.2 Add mongo into OGRRegisterAll
Edit `ogr/ogrsf_frmts/genetic/ogrregisterall.cpp`, add the `RegisterOGRMONGO()` function defined in the `ogrmongodriver.cpp`.

```c++
#ifdef MONGO_ENABLED
RegisterOGRMONGO();
#endif
```

## 2.3 Add mongo into `ogrsf_frmts.h
Edit `ogr/ogrsf_frmts/ogrsf_frmts.h`, add the `RegisterOGRMONGO()` function defined in the `ogrmongodriver.cpp`.

```c++
void CPL_DLL RegisterOGRMONGO();
```

# 3. Incorporate mongo into makefile
## 3.1 Enable Mongo
Edit `ogr/ogrsf_frmts/generic/GNUmakefile`, add the following code:

```GNUmake
ifeq ($(HAVE_MONGO),yes)
CXXFLAGS := $(CXXFLAGS) -DMONGO_ENABLED
endif
```
Which means that if `HAVE_MONGO` is defined, `MONGO` will be enabled (`Define MONGO_ENABLED`).

## 3.2 Tell compiler to include mongo folder
Edit `ogr/ogrsf_frmts/GNUmakefile`, add the following code below the paragraph.

```GNUMake
SUBDIRS-$(HAVE_MONGO) += mongo 
```
The foreach sentence after the paragraph will compile each folder contained in the `SUBDIRS-yes`.

## 3.3 MongoDB Libs
Edit the configure file, add the following code in,

```bash
HAVE_MONGO=yes
MONGO_INC="-I/opt/mongo"
MONGO_LIB="-L/opt/mongo"
```

# 4. Compile MongoDB Driver into GDAL
## 4.1 Generate GUNmakefile.opt
run configure script to generate GDALmakefile.opt, and you will find:

```bash
HAVE_MONGO = yes
MONGO_LIB = -L/opt/mongo
MONGO_INC = -I/opt/mongo
```
As we have the MongoDB C++ driver in the mongo folder, let's say put it under `/opt`. So we have to make a little change to these settings:

```bash
HAVE_MONGO = yes
MONGO_LIB = /opt/mongo/lib
MONGO_INC = -I/opt/mongo/include
GDAL_INCLUDE += $(MONGO_INC)
LIBS += $(MONGO_LIB)/libmongoclient.a \
$(MONGO_LIB)/libboost_thread.a \
$(MONGO_LIB)/libboost_system.a \
$(MONGO_LIB)/libboost_program_options.a \
$(MONGO_LIB)/libboost_filesystem.a
```

These settings will tell the compiler where to find MongoDB headers and libs, otherwise you will get lots of compiling error.

## 4.2 Install GDAL
use make and make install with options to install GDAL. After that find ogr2ogr utility and have a test.

``` bash
# ./ogr2ogr --formats
Supported Formats:
-> <strong>"MongoDB" (read/write)</strong>
-> "ESRI Shapefile" (read/write)
-> "MapInfo File" (read/write)
-> "UK .NTF" (readonly)
-> "SDTS" (readonly)
-> "TIGER" (read/write)
-> "S57" (read/write)
-> "DGN" (read/write)
-> ...
```
So MongoDB OGR Driver is ready for use.