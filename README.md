gdal
====

making gdal supported by mongodb

we are working to push the mongodb driver into gdal main release.
you may find the ticket in the gdal.org. 
http://trac.osgeo.org/gdal/ticket/5453


Right now the GDAL Driver for MongoDB hasn't been built into GDAL official release (we are working on that). The wiki here is determined to guide you compiling it with GDAL source code, so that you won't have to wait. the whole procedure is conducted in Linux OS.
<h1>1. Preparations</h1>
<h2>1.1 Download Source Codes</h2>
Download MongoDB GDAL Driver from<a href="https://github.com/mongogis/mongodb-gdal-driver"> the MongoGIS github, </a><a href="http://www.mongodb.org/downloads">MongoDB source code</a>, <a href="http://trac.osgeo.org/gdal/wiki/DownloadSource">GDAL source code</a>.
<ul>
	<li> mongodb-gdal-driver-master.zip</li>
	<li> mongodb-src-r2.4.10.zip</li>
	<li> gdal192.zip</li>
</ul>
Note: after MongoDB version 2.6.0, you could not get CXX driver from its source code. See the <a href="https://github.com/mongodb/mongo-cxx-driver">cxx-driver in github</a>.
<h2>1.2 Unzip Code Files</h2>
unzip those zip files, get three folders, mongodb-src-r2.6.3, gdal-1.11.0, mongodb-gdal-driver-master.
<pre> <code>unzip *.zip </code></pre>

<h2>1.3 Compile MongoDB C++ Driver</h2>
GDAL Driver will need the include files and lib from MongoDB,so we have to make it ready for use. Go to mongodb-src-r2.6.3 folder, and run, (you have to prepare the <a href="http://www.scons.org/">SCons </a>to build MongoDB.)
<pre> <code> scons --prefix=/opt/mongo install --full</code></pre>

--prefix is for the installation directory, and --full enables the “full” installation, directing SCons to install the driver <strong>headers and libraries</strong> to the prefix directory. more info in <a href="https://github.com/mongodb/mongo-cxx-driver/wiki/Download%20and%20Compile#build-options">github</a> and <a href="http://www.mongodb.org/about/contributors/tutorial/build-mongodb-from-source/">mongodb.org</a>. After the building is done, you will get three folders in the /opt/mongo - <strong>bin</strong>, <strong>include</strong>, <strong>libs</strong>, cope the boost library under the mongodb source code (src/third_party/boost) into the <strong>include</strong> folder, cope the *.a files in the build/linux2/normal/third_party/boost/ folder into <strong>libs</strong>.

so in the end you got a mongo folder going like this:
<ul>
	<li>bin</li>
	<li>include  |  mongo, boost</li>
	<li>lib   |  libboost_filesystem.a libboost_program_options.a libboost_system.a libboost_thread.a libmongoclient.a</li>
</ul>

<h1>2. Incorporate mongo into GDAL architecture</h1>
<h2>2.1 Move into ogrsf_frmts folder</h2>
move mongodb-gdal-driver-master into gdal ogrsf_frmts folder and rename it as mongo.
<pre> <code>mv mongodb-gdal-driver-master/ gdal-1.11.0/ogr/ogrsf_frmts/mongo </code></pre>

<h2>2.2 Add mongo into OGRRegisterAll</h2>
Edit ogr/ogrsf_frmts/genetic/ogrregisterall.cpp, add the RegisterOGRMONGO() function defined in the ogrmongodriver.cpp.
<pre><code>#ifdef MONGO_ENABLED
RegisterOGRMONGO();
#endif</code></pre>

<h2>2.3 Add mongo into ogrsf_frmts.h</h2>
Edit ogr/ogrsf_frmts/ogrsf_frmts.h, add the RegisterOGRMONGO() function defined in the ogrmongodriver.cpp.
<pre><code>void CPL_DLL RegisterOGRMONGO();</code></pre>

<h1>3. Incorporate mongo into makefile</h1>
<h2>3.1 Enable Mongo</h2>
Edit ogr/ogrsf_frmts/generic/GNUmakefile, add the following code,
<pre><code>ifeq ($(HAVE_MONGO),yes)
CXXFLAGS := $(CXXFLAGS) -DMONGO_ENABLED
endif</code></pre>

which means that if HAVE_MONGO is defined, MONGO will be enabled (Define MONGO_ENABLED).
<h2>3.2 Tell compiler to include mongo folder</h2>
Edit ogr/ogrsf_frmts/GNUmakefile, add the following code below the paragraph.
<pre><code>SUBDIRS-$(HAVE_MONGO) += mongo </code></pre>

The foreach sentence after the paragraph will compile each folder contained in the SUBDIRS-yes.
<h2>3.3 MongoDB Libs</h2>
Edit the configure file, add the following code in,
<pre><code>HAVE_MONGO=yes
MONGO_INC="-I/opt/mongo"
MONGO_LIB="-L/opt/mongo"</code></pre>

<h1>4. Compile MongoDB Driver into GDAL</h1>
<h2>4.1 Generate GUNmakefile.opt</h2>
run configure script to generate GDALmakefile.opt, and you will find,
<pre><code>HAVE_MONGO = yes
MONGO_LIB = -L/opt/mongo
MONGO_INC = -I/opt/mongo</code></pre>

As we have the MongoDB C++ driver in the mongo folder, let's say put it under /opt. so we have to make a little change to these settings.
<pre><code>HAVE_MONGO = yes
MONGO_LIB = /opt/mongo/lib
MONGO_INC = -I/opt/mongo/include
GDAL_INCLUDE += $(MONGO_INC)
LIBS += $(MONGO_LIB)/libmongoclient.a \
$(MONGO_LIB)/libboost_thread.a \
$(MONGO_LIB)/libboost_system.a \
$(MONGO_LIB)/libboost_program_options.a \
$(MONGO_LIB)/libboost_filesystem.a</code></pre>

These settings will tell the compiler where to find MongoDB headers and libs, otherwise you will get lots of compiling error.
<h2>4.2 Install GDAL</h2>
use make and make install with options to install GDAL. After that find ogr2ogr utility and have a test.
<pre><code># ./ogr2ogr --formats
Supported Formats:
-&gt; <strong>"MongoDB" (read/write)</strong>
-&gt; "ESRI Shapefile" (read/write)
-&gt; "MapInfo File" (read/write)
-&gt; "UK .NTF" (readonly)
-&gt; "SDTS" (readonly)
-&gt; "TIGER" (read/write)
-&gt; "S57" (read/write)
-&gt; "DGN" (read/write)
-&gt; ...</code></pre>

So MongoDB OGR Driver is ready for use.

