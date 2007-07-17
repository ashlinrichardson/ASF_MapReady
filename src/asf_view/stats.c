#include "asf_view.h"

// global variable for the image stats
ImageStats g_stats;

void clear_stats()
{
    g_stats.avg = 0.0;
    g_stats.stddev = 0.0;

    g_stats.act_max = 0.0;
    g_stats.act_min = 0.0;

    g_stats.map_min = 0.0;
    g_stats.map_max = 0.0;

    int ii;
    for (ii=0; ii<256; ++ii)
        g_stats.hist[ii] = 0;
}

// Now the thumbnail generation is combined with the stats calculations,
// to speed things up a little.  Both require a pass through the entire
// image, so it seems natural, and the stats calculation doesn't add much
// overhead.  (The user is waiting while the thumbnail is being generated,
// so we do want this to be as quick as possible.)
unsigned char *generate_thumbnail_data(int tsx, int tsy)
{
    int ii, jj;
    unsigned char *bdata = MALLOC(sizeof(unsigned char)*tsx*tsy*3);

    // we will estimate the stats from the thumbnail data
    clear_stats();

    // Here we do the rather ugly thing of making the thumbnail
    // loading code specific to each supported data type.  This is
    // because we've combined the stats calculation into this...
    if (data_ci->data_type == GREYSCALE_FLOAT) {
        // store data used to build the small image pixmap
        // we will calculate the stats on this subset
        float *fdata = MALLOC(sizeof(float)*tsx*tsy);

        load_thumbnail_data(data_ci, tsx, tsy, fdata);

        g_stats.act_max = g_stats.act_min = fdata[0];

        // split out the case where we have no ignore value --
        // should be quite a bit faster...
        if (meta_is_valid_double(meta->general->no_data)) {
            // Compute stats -- ignore "no data" value
            int n=0;
            for ( ii = 0 ; ii < tsy ; ii++ ) {
                for ( jj = 0 ; jj < tsx ; jj++ ) {
                    float v = fdata[jj+ii*tsx];
                    if (v != meta->general->no_data) {
                        g_stats.avg += v;
                        if (v > g_stats.act_max) g_stats.act_max = v;
                        if (v < g_stats.act_min) g_stats.act_min = v;
                        ++n;
                    }
                }
            }
            g_stats.avg /= (double)n;
            for ( ii = 0 ; ii < tsy ; ii++ ) {
                for ( jj = 0 ; jj < tsx ; jj++ ) {
                    float v = fdata[jj+ii*tsx];
                    if (v != meta->general->no_data)
                        g_stats.stddev += (v - g_stats.avg) * (v - g_stats.avg);
                }
            }
            g_stats.stddev = sqrt(g_stats.stddev / (double)(tsx*tsy));
        } else {
            // Compute stats -- no ignore
            for ( ii = 0 ; ii < tsy ; ii++ ) {
                for ( jj = 0 ; jj < tsx ; jj++ ) {
                    float v = fdata[jj+ii*tsx];
                    g_stats.avg += v;
                    if (v > g_stats.act_max) g_stats.act_max = v;
                    if (v < g_stats.act_min) g_stats.act_min = v;
                }
            }
            g_stats.avg /= (double)(tsx*tsy);
            for ( ii = 0 ; ii < tsy ; ii++ ) {
                for ( jj = 0 ; jj < tsx ; jj++ ) {
                    float v = fdata[jj+ii*tsx];
                    g_stats.stddev += (v - g_stats.avg) * (v - g_stats.avg);
                }
            }
            g_stats.stddev = sqrt(g_stats.stddev / (double)(tsx*tsy));
        }

        //printf("Avg, StdDev: %f, %f\n", g_stats.avg, g_stats.stddev);

        // Set the limits of the scaling - 2-sigma on either side of the mean
        // These are globals, we will use them in the big image, too.
        g_stats.map_min = g_stats.avg - 2*g_stats.stddev;
        g_stats.map_max = g_stats.avg + 2*g_stats.stddev;

        // Now actually scale the data, and convert to bytes.
        // Note that we need 3 values, one for each of the RGB channels.
        int have_no_data = meta_is_valid_double(meta->general->no_data);
        for ( ii = 0 ; ii < tsy ; ii++ ) {
            for ( jj = 0 ; jj < tsx ; jj++ ) {
                int index = jj+ii*tsx;
                float val = fdata[index];

                unsigned char uval;
                if (have_no_data && val == meta->general->no_data)
                    uval = 0;
                else if (val < g_stats.map_min)
                    uval = 0;
                else if (val > g_stats.map_max)
                    uval = 255;
                else
                    uval = (unsigned char)(((val-g_stats.map_min)/(g_stats.map_max-g_stats.map_min))*255+0.5);
            
                int n = 3*index;
                bdata[n] = uval;
                bdata[n+1] = uval;
                bdata[n+2] = uval;

                g_stats.hist[uval] += 1;
            }
        }

        // done with our subset
        free(fdata);
    }
    else if (data_ci->data_type == RGB_BYTE) {

        load_thumbnail_data(data_ci, tsx, tsy, (void*)bdata);

        // initialize to opposite extrema
        g_stats.act_max = 0;
        g_stats.act_min = 255;

        // don't really have a mapping for byte data...
        g_stats.map_min = 0;
        g_stats.map_max = 255;

        for ( ii = 0 ; ii < tsy ; ii++ ) {
            for ( jj = 0 ; jj < tsx ; jj++ ) {
                int kk = 3*(jj+ii*tsx);
                unsigned char uval = 
                    (bdata[kk] + bdata[kk+1] + bdata[kk+2])/3;

                g_stats.avg += uval;
                g_stats.hist[uval] += 1;

                if (uval > g_stats.act_max) g_stats.act_max = uval;
                if (uval < g_stats.act_min) g_stats.act_min = uval;
            }
        }

        g_stats.avg /= (double)(tsx*tsy);

        for ( ii = 0 ; ii < tsy ; ii++ ) {
            for ( jj = 0 ; jj < tsx ; jj++ ) {
                int kk = 3*(jj+ii*tsx);
                unsigned char uval = 
                    (bdata[kk] + bdata[kk+1] + bdata[kk+2])/3;

                g_stats.stddev += (uval - g_stats.avg) * (uval - g_stats.avg);
            }
        }
        g_stats.stddev = sqrt(g_stats.stddev / (double)(tsx*tsy));

    }
    else if (data_ci->data_type == GREYSCALE_BYTE) {

        // this case is very similar to the RGB case, above, except we
        // have to first grab the data into a greyscale buffer, and
        // then copy it over to the 3-band buffer we're supposed to return
        unsigned char *gsdata = MALLOC(sizeof(unsigned char)*tsx*tsy);
        load_thumbnail_data(data_ci, tsx, tsy, (void*)gsdata);

        g_stats.act_max = 0;
        g_stats.act_min = 255;
        g_stats.map_min = 0;
        g_stats.map_max = 255;

        for ( ii = 0 ; ii < tsy ; ii++ ) {
            for ( jj = 0 ; jj < tsx ; jj++ ) {
                unsigned char uval = gsdata[jj+ii*tsx];
                int index = 3*(jj+ii*tsx);
                bdata[index] = bdata[index+1] = bdata[index+2] = uval;

                g_stats.avg += uval;
                g_stats.hist[uval] += 1;

                if (uval > g_stats.act_max) g_stats.act_max = uval;
                if (uval < g_stats.act_min) g_stats.act_min = uval;
            }
        }

        g_stats.avg /= (double)(tsx*tsy);

        for ( ii = 0 ; ii < tsy ; ii++ ) {
            for ( jj = 0 ; jj < tsx ; jj++ ) {
                unsigned char uval = gsdata[jj+ii*tsx];
                g_stats.stddev += (uval - g_stats.avg) * (uval - g_stats.avg);
            }
        }
        g_stats.stddev = sqrt(g_stats.stddev / (double)(tsx*tsy));

        free(gsdata);
    }

    return bdata;
}

int calc_scaled_pixel_value(float val)
{
    if (meta_is_valid_double(meta->general->no_data) && 
        val == meta->general->no_data)
        return 0;
    if (val < g_stats.map_min)
        return 0;
    else if (val > g_stats.map_max)
        return 255;
    else
        return (int) round(((val-g_stats.map_min)/
            (g_stats.map_max-g_stats.map_min))*255);
}

static void fill_stats_label()
{
    char s[1024];
    strcpy(s, "");

    // y = m*x + b
    double m = 255.0/(g_stats.map_max-g_stats.map_min);
    double b = -g_stats.map_min*255.0/(g_stats.map_max-g_stats.map_min);

    // we will take charge of displaying the sign
    char c = b>0 ? '+' : '-';
    b = fabs(b);

    // Not sure we should put the Max/Min in here... after all, these
    // are only from a subset.  The aggregate values (avg, stddev, mapping)
    // will be fine, but max/min could be quite far off, if there are
    // an outlier or two.
    sprintf(&s[strlen(s)],
        "Average: %.3f\n"
        "Standard Deviation: %.3f\n"
        "Min Value: %.2f\n"
        "Max Value: %.2f\n"
        "Mapping Fn for pixels:\n"
        "  Y = %.3f * X %c %.3f",
        g_stats.avg, g_stats.stddev,
        g_stats.act_min, g_stats.act_max, 
        m, c, b);

    put_string_to_label("stats_label", s);
}

static void destroy_pb_data(guchar *pixels, gpointer data)
{
    free(pixels);
}

static void pop_hist()
{
    int i,j;

    int bin_max = 0;
    for (i=0; i<256; ++i) {
        if (g_stats.hist[i] > bin_max) bin_max = g_stats.hist[i];
    }

    const int w = 200;
    unsigned char *histogram_data = MALLOC(sizeof(unsigned char)*256*w*4);
    for (i=0; i<256; ++i) {
        int l = (int)((double)g_stats.hist[i] / (double)bin_max * (double)w);
        for (j=0; j<l*4; j += 4) {
            histogram_data[j+i*w*4] = (unsigned char)0;
            histogram_data[j+i*w*4+1] = (unsigned char)0;
            histogram_data[j+i*w*4+2] = (unsigned char)0;
            histogram_data[j+i*w*4+3] = (unsigned char)255; // alpha
        }
        for (j=l*4; j<w*4; j+=4) {
            histogram_data[j+i*w*4] = (unsigned char)0;
            histogram_data[j+i*w*4+1] = (unsigned char)0;
            histogram_data[j+i*w*4+2] = (unsigned char)0;
            histogram_data[j+i*w*4+3] = (unsigned char)0;   // alpha
        }
    }

    GdkPixbuf *pb = gdk_pixbuf_new_from_data(histogram_data,
        GDK_COLORSPACE_RGB, TRUE, 8, w, 256, w*4, destroy_pb_data, NULL);

    GtkWidget *img = get_widget_checked("histogram_image");

    GdkPixbuf *old_pb = gtk_image_get_pixbuf(GTK_IMAGE(img));
    g_object_unref(old_pb);

    gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);

}

int fill_stats()
{
    int i, j;

    const int w = 12; // width of the little "scale" image
    unsigned char *histogram_scale_data = MALLOC(sizeof(unsigned char)*256*w*3);
    for (i=0; i<256; ++i) {
        for (j=0; j<w*3; ++j) {
            histogram_scale_data[j+i*w*3] = (unsigned char)i;
        }
    }

    GdkPixbuf *pb = gdk_pixbuf_new_from_data(histogram_scale_data,
        GDK_COLORSPACE_RGB, FALSE, 8, w, 256, w*3, destroy_pb_data, NULL);

    GtkWidget *img = get_widget_checked("histogram_scale_image");

    GdkPixbuf *old_pb = gtk_image_get_pixbuf(GTK_IMAGE(img));
    g_object_unref(old_pb);

    gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);

    fill_stats_label();
    pop_hist();

    return TRUE;
}