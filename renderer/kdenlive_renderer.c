/*

	Customized version of MLT's inigo, used to render timeline to a file

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#include <mlt/framework/mlt.h>

static mlt_consumer create_consumer( char *id, mlt_producer producer )
{
	char *arg = id != NULL ? strchr( id, ':' ) : NULL;
	if ( arg != NULL )
		*arg ++ = '\0';
	mlt_consumer consumer = mlt_factory_consumer( id, arg );
	if ( consumer != NULL )
	{
		mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );
		mlt_properties_set_data( properties, "transport_producer", producer, 0, NULL, NULL );
		mlt_properties_set_data( MLT_PRODUCER_PROPERTIES( producer ), "transport_consumer", consumer, 0, NULL, NULL );
	}
	return consumer;
}

static void transport( mlt_producer producer, mlt_consumer consumer )
{
	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer );
	int stats = mlt_properties_get_int( MLT_CONSUMER_PROPERTIES( consumer ), "stats_on" );
	int pos = 0;
	int currentPos = 0;
	int totalLength = (int)mlt_producer_get_length( producer );
	/*struct timespec tm { 0, 40000 };*/

	if ( mlt_properties_get_int( properties, "done" ) == 0 && !mlt_consumer_is_stopped( consumer ) )
	{
		while( mlt_properties_get_int( properties, "done" ) == 0 && !mlt_consumer_is_stopped( consumer ) )
		{
			if ( stats != 0 ) {
				currentPos = (int)mlt_producer_position( producer );
				if (currentPos > pos) {
					fprintf( stderr, "Current Frame %10d, percentage: %10d\r", currentPos, (int) 100 * currentPos / totalLength);
					pos = currentPos;
				}
			}
/*			if ( !silent )
				nanosleep( &tm, NULL );*/
		}
		fprintf( stderr, "\n" );
	}
}

int main( int argc, char **argv )
{
	int i;
	mlt_consumer consumer = NULL;
	mlt_producer inigo = NULL;
	FILE *store = NULL;
	char *name = NULL;
	struct sched_param scp;

	/* Use realtime scheduling if possible 
	memset( &scp, '\0', sizeof( scp ) );
	scp.sched_priority = sched_get_priority_max( SCHED_FIFO ) - 1;
	#ifndef __DARWIN__
	sched_setscheduler( 0, SCHED_FIFO, &scp );
	#endif*/

	/* Construct the factory */
	mlt_factory_init( NULL );

	/* Check for serialisation switch first */
	for ( i = 1; i < argc; i ++ )
	{
		if ( !strcmp( argv[ i ], "-serialise" ) )
		{
			name = argv[ ++ i ];
			if ( strstr( name, ".inigo" ) )
				store = fopen( name, "w" );
		}
	}


	/* Get inigo producer */
	if ( argc > 1 )
		inigo = mlt_factory_producer( "inigo", &argv[ 1 ] );

	if ( argc > 1 && inigo != NULL && mlt_producer_get_length( inigo ) > 0 )
	{
		/* Get inigo's properties */
		mlt_properties inigo_props = MLT_PRODUCER_PROPERTIES( inigo );

		/* Get the last group */
		mlt_properties group = mlt_properties_get_data( inigo_props, "group", 0 );

		/* Parse the arguments */
		for ( i = 1; i < argc; i ++ )
		{
			if ( !strcmp( argv[ i ], "-consumer" ) )
			{
				consumer = create_consumer( argv[ ++ i ], inigo );
				while ( argv[ i + 1 ] != NULL && strstr( argv[ i + 1 ], "=" ) )
					mlt_properties_parse( group, argv[ ++ i ] );
			}
			else if ( !strcmp( argv[ i ], "-serialise" ) )
			{
				i ++;
			}
			else
			{
				if ( store != NULL )
					fprintf( store, "%s\n", argv[ i ] );

				i ++;

				while ( argv[ i ] != NULL && argv[ i ][ 0 ] != '-' )
				{
					if ( store != NULL )
						fprintf( store, "%s\n", argv[ i ] );
					i += 1;
				}

				i --;
			}
		}

		/* If we have no consumer, default to sdl */
		if ( store == NULL && consumer == NULL )
			consumer = create_consumer( NULL, inigo );

		if ( consumer != NULL && store == NULL )
		{
			/* Apply group settings */
			mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );
			mlt_properties_inherit( properties, group );

			/* Connect consumer to inigo */
			mlt_consumer_connect( consumer, MLT_PRODUCER_SERVICE( inigo ) );

			/* Start the consumer */
			mlt_consumer_start( consumer );

			/* Transport functionality */
			transport( inigo, consumer );

			/* Stop the consumer */
			mlt_consumer_stop( consumer );
		}
		else if ( store != NULL )
		{
			fprintf( stderr, "Project saved as %s.\n", name );
			fclose( store );
		}
	}
	else
	{
		fprintf( stderr, "Usage: kdenlive_renderer [ -group [ name=value ]* ]\n"
						 "             [ -consumer id[:arg] [ name=value ]* ]\n"
						 "             [ -filter filter[:arg] [ name=value ] * ]\n"
						 "             [ -attach filter[:arg] [ name=value ] * ]\n"
						 "             [ -mix length [ -mixer transition ]* ]\n"
						 "             [ -transition id[:arg] [ name=value ] * ]\n"
						 "             [ -blank frames ]\n"
						 "             [ -track ]\n"
						 "             [ -split relative-frame ]\n"
						 "             [ -join clips ]\n"
						 "             [ -repeat times ]\n"
						 "             [ producer [ name=value ] * ]+\n" );
	}

	/* Close the consumer */
	if ( consumer != NULL )
		mlt_consumer_close( consumer );

	/* Close the producer */
	if ( inigo != NULL )
		mlt_producer_close( inigo );

	/* Close the factory */
	mlt_factory_close( );

	return 0;
}
