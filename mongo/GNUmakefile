# $Id$
#
# Makefile to build OGR MongoDB driver
#
include ../../../GDALmake.opt

OBJ =  ogrmongodriver.o ogrmongodatasource.o ogrmongolayer.o ogrmongoreader.o

CPPFLAGS        :=      -I.. -I../.. -I../geojson -I../geojson/jsonc $(GDAL_INCLUDE) $(CPPFLAGS) $(MONGO_INC)

default:   $(O_OBJ:.o=.$(OBJ_EXT))

clean:
        rm -f *.o $(O_OBJ)

$(O_OBJ):   ogr_mongo.h ogrmongoreader.h ../geojson/ogrgeojsonwriter.h
