// read_image_X11.cpp - Reads images using X11 API
// Created by ao on 6/12/24.
//


//#!cc -Werror -std=c99 -I/usr/include/libdrm -ldrm enum.c -o enum && ./enum /dev/dri/card0
#include <vector>
#include "read_image_X11.h"
#include <string.h>

// comment the next line to busy-wait at each frame
//#define __SLEEP__
#define FRAME  0
#define PERIOD 1000000
#define NAME   "screencap"
#define NAMESP "         "
#define BPP    4



void initimage( struct shmimage * image )
{
    image->ximage = NULL ;
    image->shminfo.shmaddr = (char *) -1 ;
}

void destroyimage( Display * dsp, struct shmimage * image )
{
    if( image->ximage )
    {
        XShmDetach( dsp, &image->shminfo ) ;
        XDestroyImage( image->ximage ) ;
        image->ximage = NULL ;
    }

    if( image->shminfo.shmaddr != ( char * ) -1 )
    {
        shmdt( image->shminfo.shmaddr ) ;
        image->shminfo.shmaddr = ( char * ) -1 ;
    }
}

int createimage( Display * dsp, struct shmimage * image, int width, int height )
{
    // Create a shared memory area
    image->shminfo.shmid = shmget( IPC_PRIVATE, width * height * BPP, IPC_CREAT | 0600 ) ;
    if( image->shminfo.shmid == -1 )
    {
        perror( NAME ) ;
        return false ;
    }

    // Map the shared memory segment into the address space of this process
    image->shminfo.shmaddr = (char *) shmat( image->shminfo.shmid, 0, 0 ) ;
    if( image->shminfo.shmaddr == (char *) -1 )
    {
        perror( NAME ) ;
        return false ;
    }

    image->data = (unsigned int*) image->shminfo.shmaddr ;
    image->shminfo.readOnly = false ;

    // Mark the shared memory segment for removal
    // It will be removed even if this program crashes
    shmctl( image->shminfo.shmid, IPC_RMID, 0 ) ;

    // Allocate the memory needed for the XImage structure
    image->ximage = XShmCreateImage( dsp, XDefaultVisual( dsp, XDefaultScreen( dsp ) ),
                                     DefaultDepth( dsp, XDefaultScreen( dsp ) ), ZPixmap, 0,
                                     &image->shminfo, 0, 0 ) ;
    if( !image->ximage )
    {
        destroyimage( dsp, image ) ;
        printf( NAME ": could not allocate the XImage structure\n" ) ;
        return false ;
    }

    image->ximage->data = (char *)image->data ;
    image->ximage->width = width ;
    image->ximage->height = height ;

    // Ask the X server to attach the shared memory segment and sync
    XShmAttach( dsp, &image->shminfo ) ;
    XSync( dsp, false ) ;
    return true ;
}

void getrootwindow( Display * dsp, struct shmimage * image )
{
    XShmGetImage( dsp, XDefaultRootWindow( dsp ), image->ximage, 0, 0, AllPlanes ) ;
}

long timestamp( )
{

    return (unsigned long)time(NULL);
}

Window createwindow( Display * dsp, int width, int height )
{
    unsigned long mask = CWBackingStore ;
    XSetWindowAttributes attributes ;
    attributes.backing_store = NotUseful ;
    mask |= CWBackingStore ;
    Window window = XCreateWindow( dsp, DefaultRootWindow( dsp ),
                                   0, 0, width, height, 0,
                                   DefaultDepth( dsp, XDefaultScreen( dsp ) ),
                                   InputOutput, CopyFromParent, mask, &attributes ) ;
    XStoreName( dsp, window, NAME );
    XSelectInput( dsp, window, StructureNotifyMask ) ;
    XMapWindow( dsp, window );
    return window ;
}

void destroywindow( Display * dsp, Window window )
{
    XDestroyWindow( dsp, window );
}

unsigned int getpixel( struct shmimage * src, struct shmimage * dst,
                       int j, int i, int w, int h )
{
    int x = (float)(i * src->ximage->width) / (float)w ;
    int y = (float)(j * src->ximage->height) / (float)h ;
    return src->data[ y * src->ximage->width + x ] ;
}

std::vector<unsigned int> processimage( struct shmimage * src, struct shmimage * dst )
{
    int sw = src->ximage->width ;
    int sh = src->ximage->height ;
    int dw = dst->ximage->width ;
    int dh = dst->ximage->height ;

    std::cout << "src: " << sw << " x " << sh << std::endl;
    std::cout << "dst: " << dw << " x " << dh << std::endl;

    // Here you can set the resulting position and size of the captured screen
    // Because of the limitations of this example, it must fit in dst->ximage
    int w = dw  ;
    int h = dh  ;
    int x = ( dw - w ) ;
    int y = ( dh - h ) / 2 ;

    // Just in case...
    if( x < 0 || y < 0 || x + w > dw || y + h > dh || sw < dw || sh < dh )
    {
        printf( NAME   ": This is only a limited example\n" ) ;
        printf( NAMESP "  Please implement a complete scaling algorithm\n" ) ;
        throw false ;
    }

    unsigned int * d = dst->data + y * dw + x ; // old code
//    unsigned int* d = new unsigned int[w * h]; // Allocate memory for d
    std::cout << "Length of array: " << w*h << std::endl;
    std::vector<unsigned int> d_vect;
    d_vect.resize(w*h);

//    d += y * dw + x;
    // Calculate the index in the newly allocated memory similar to dst->data + y * dw + x
//    unsigned int* d_ptr = d + y * dw + x;

    //unsigned int* d_ptr = d;


    int r = dw - w ;
    int j, i ;
    for( j = 0 ; j < h ; ++j )
    {
        for( i = 0 ; i < w ; ++i )
        {
            *d++ = getpixel( src, dst, j, i, w, h ) ;
            d_vect[j*w + i] = *(d - 1);
        }
        d += r ;
    }
    return d_vect ;
}


std::vector<std::vector<unsigned int>> run( Display * dsp, Window window, struct shmimage * src, struct shmimage * dst )
{
    std::vector<shmimage *> results;
    std::vector<std::vector<unsigned int>> results_raw;
    results.resize(30);
    results_raw.resize(30);

    // collect an array of 60 images
    XGCValues xgcvalues ;
    xgcvalues.graphics_exposures = False ;
    GC gc = XCreateGC( dsp, window, GCGraphicsExposures, &xgcvalues ) ;

    Atom delete_atom = XInternAtom( dsp, "WM_DELETE_WINDOW", False ) ;
    XSetWMProtocols( dsp, window, &delete_atom, True ) ;

    XEvent xevent ;
    int running = true ;
    int initialized = false ;
    int dstwidth = dst->ximage->width ;
    int dstheight = dst->ximage->height ;
    long framets = timestamp( );
    long periodts = timestamp( ) ;
    int results_idx = 0;

    std::cout << framets << " " << periodts << std::endl;

    long frames = 0 ;
    int fd = ConnectionNumber( dsp ) ;
    while( running )
    {
        std::cout << "running loop" << std::endl;
        while( XPending( dsp ) )
        {
            std::cout << "xpending" << std::endl;
            XNextEvent( dsp, &xevent ) ;
            if( ( xevent.type == ClientMessage && xevent.xclient.data.l[0] == delete_atom )
                || xevent.type == DestroyNotify )
            {
                running = false ;
                break ;
            }
            else if( xevent.type == ConfigureNotify )
            {
                if( xevent.xconfigure.width == dstwidth
                    && xevent.xconfigure.height == dstheight )
                {
                    initialized = true ;
                }
            }
        }
        if( initialized )
        {
            getrootwindow( dsp, src ) ;


            std::vector<unsigned int> image_buf = processimage( src, dst );

//            std::cout << dst->ximage->width << " x " << dst->ximage->height << std::endl;
            // deep copy of dst
//            shmimage * dst_cpy = dst;
//            dst_cpy->data = new unsigned int[dstwidth * dstheight];
//            dst_cpy->ximage = new XImage(dst->ximage);
//            memcpy(dst_cpy->data, dst->data, dstwidth * dstheight * sizeof(unsigned int));
//
//            dst_cpy->shminfo = dst->shminfo;
//
//            dst_cpy->ximage->data = (char *)dst_cpy->data;
//            dst_cpy->ximage->width = dstwidth;
//            dst_cpy->ximage->height = dstheight;




            //Also put ximage in the result array
            results[results_idx] = dst;
            results_raw[results_idx] = image_buf;
            results_idx++;
            if (results_idx > 29) {
                return results_raw;
            }

            XShmPutImage( dsp, window, gc, dst->ximage,
                          0, 0, 0, 0, dstwidth, dstheight, False ) ;
            XSync( dsp, False ) ;

            int frameus = timestamp( ) - framets ;
            ++frames ;
            while( frameus < FRAME )
            {
                std::cout << frameus << "<" << FRAME << std::endl;
#if defined( __SLEEP__ )
                usleep( FRAME - frameus ) ;
#endif
                frameus = timestamp( ) - framets ;
            }
            framets = timestamp( ) ;

            int periodus = timestamp( ) - periodts ;
            if( periodus >= PERIOD )
            {
                printf( "fps: %d\n", (int) (1000000.0L * frames / periodus ) ) ;
                frames = 0 ;
                periodts = framets ;
            }
        }
    }
    return std::vector<std::vector<unsigned int>>() ;
}


struct display_nfo {
    int dstwidth;
    int dstheight;
};

struct display_nfo initialize_xserver(bool debug, Display * &dsp, struct shmimage &src, struct shmimage &dst) {

    dsp = XOpenDisplay( NULL ) ;
    if( !dsp )
    {
        printf( NAME ": could not open a connection to the X server\n" ) ;
        throw 1;
    }

    if( !XShmQueryExtension( dsp ) )
    {
        XCloseDisplay( dsp ) ;
        printf( NAME ": the X server does not support the XSHM extension\n" ) ;
        throw 1 ;
    }

    int screen = XDefaultScreen( dsp ) ;
    initimage( &src ) ;
    int width = XDisplayWidth( dsp, screen ) ;
    int height = XDisplayHeight( dsp, screen ) ;
    if( !createimage( dsp, &src, width, height ) )
    {
        XCloseDisplay( dsp ) ;
        throw 1 ;
    }
    initimage( &dst ) ;
    int dstwidth = width / 2 ;
    int dstheight = height / 2 ;
    if( !createimage( dsp, &dst, dstwidth, dstheight ) )
    {
        destroyimage( dsp, &src ) ;
        XCloseDisplay( dsp ) ;
        throw 1 ;
    }

    if( dst.ximage->bits_per_pixel != 32 )
    {
        destroyimage( dsp, &src ) ;
        destroyimage( dsp, &dst ) ;
        XCloseDisplay( dsp ) ;
        printf( NAME   ": This is only a limited example\n" ) ;
        printf( NAMESP "  Please add support for all pixel formats using: \n" ) ;
        printf( NAMESP "      dst.ximage->bits_per_pixel\n" ) ;
        printf( NAMESP "      dst.ximage->red_mask\n" ) ;
        printf( NAMESP "      dst.ximage->green_mask\n" ) ;
        printf( NAMESP "      dst.ximage->blue_mask\n" ) ;
        throw 1 ;
    }

    display_nfo ret = {
            dstwidth,
            dstheight
    };

    return ret;

}



//Main entrypoint to read image
std::vector<std::vector<unsigned int>> read_image_from_xserver(bool debug) {
//    Display * dsp;
//    struct shmimage src, dst ;
//    display_nfo d_nfo = initialize_xserver(true, dsp, src, dst);
//
//    // Debug of display parameters
//    std::cout << "X Server Parameters:" << std::endl;
//    std::cout << dsp << std::endl;
//    std::cout << d_nfo.dstwidth << " x " << d_nfo.dstwidth << std::endl << std::endl;
//
//    Window window = createwindow( dsp, d_nfo.dstheight, d_nfo.dstwidth ) ;
//
//    run( dsp, window, &src, &dst ) ;
//    destroywindow( dsp, window ) ;
//
//    destroyimage( dsp, &src ) ;
//    destroyimage( dsp, &dst ) ;
//    XCloseDisplay( dsp ) ;
//    return src.ximage ;

    Display * dsp = XOpenDisplay( NULL ) ;
    if( !dsp )
    {
        printf( NAME ": could not open a connection to the X server\n" ) ;
        throw 1 ;
    }

    if( !XShmQueryExtension( dsp ) )
    {
        XCloseDisplay( dsp ) ;
        printf( NAME ": the X server does not support the XSHM extension\n" ) ;
        throw 1 ;
    }

    int screen = XDefaultScreen( dsp ) ;
    struct shmimage src, dst ;
    initimage( &src ) ;
    int width = XDisplayWidth( dsp, screen ) ;
    int height = XDisplayHeight( dsp, screen ) ;
    if( !createimage( dsp, &src, width, height ) )
    {
        XCloseDisplay( dsp ) ;
        throw 1 ;
    }
    initimage( &dst ) ;
    int dstwidth = width / 2 ;
    int dstheight = height / 2 ;
    if( !createimage( dsp, &dst, dstwidth, dstheight ) )
    {
        destroyimage( dsp, &src ) ;
        XCloseDisplay( dsp ) ;
        throw 1 ;
    }

    if( dst.ximage->bits_per_pixel != 32 )
    {
        destroyimage( dsp, &src ) ;
        destroyimage( dsp, &dst ) ;
        XCloseDisplay( dsp ) ;
        printf( NAME   ": This is only a limited example\n" ) ;
        printf( NAMESP "  Please add support for all pixel formats using: \n" ) ;
        printf( NAMESP "      dst.ximage->bits_per_pixel\n" ) ;
        printf( NAMESP "      dst.ximage->red_mask\n" ) ;
        printf( NAMESP "      dst.ximage->green_mask\n" ) ;
        printf( NAMESP "      dst.ximage->blue_mask\n" ) ;
        throw 1 ;
    }

    Window window = createwindow( dsp, dstwidth, dstheight ) ;
    std::vector<std::vector<unsigned int>> res = run( dsp, window, &src, &dst ) ;
    destroywindow( dsp, window ) ;

    destroyimage( dsp, &src ) ;
    destroyimage( dsp, &dst ) ;
    XCloseDisplay( dsp ) ;
    return res;

}

void is_read_image_X11_alive() {
    std::cout << "read_image_X11 loaded" << std::endl;
}

