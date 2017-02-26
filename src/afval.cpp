#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PENALTY_NOT_PLANNED	3
#define PENALTY_OVER_CAPACITY	4
#define PENALTY_OVER_TIME	1

#define TABU_LENGTH		256

#define NODE_COUNT		1099
#define ORDER_COUNT		1177
#define TRUCK_COUNT		2
#define DAY_COUNT		5

#define BASE_LOCATION		287
#define RETURN_TO_BASE		50000

#define HAMMER_TIME		3000
#define MAXIMUM_CAPACITY	100000
#define MATSECONDS_PER_DAY	(8*60*100)

#define NO_DAY			0
#define MONDAY			1
#define FREE_LIST_DAY		42

#define NO_TRUCK		0
#define TRUCK_A			1
#define TRUCK_B			2
#define FREE_LIST_TRUCK		42

#define FREE_LIST		0
//#define	LOOP_COUNT		(64*5)
//#define LOOP_COUNT		11
#define LOOP_COUNT		12
//#define LOOP_COUNT		21
//#define LOOP_COUNT		256

#define LINE_LENGTH		100
#define LOCATION_LENGTH		64
#define FREQUENCY_LENGTH	16

//#define SANITY_VALUE_NOORDERS	957807
#define SANITY_VALUE_NOORDERS	1067934


int main( int argc, char* argv[] );
void initialize_solution();
void sanitize_solution();
unsigned valuate_solution();
void mutate_solution( bool trace );
void print_solution();

void read_distances_text( const char* fname );
void read_orders_text( const char* fname );
void read_distances( const char* fname );
void read_travel_time( const char* fname );
void read_orders( const char* fname );
void read_solution( const char* fname );
void write_distances( const char* fname );
void write_travel_time( const char* fname );
void write_orders( const char* fname );
void write_solution( const char* fname, bool placenames );
void write_state( const char* fname );

void dump_loop( int i );

typedef struct {
	unsigned	order_id;
	unsigned	matrix_id;
	unsigned	volume;
	unsigned	time;
	unsigned char	frequency;
} order_entry_t;

unsigned short distance[NODE_COUNT][NODE_COUNT];		// TODO: unsigned short
unsigned short travel_time[NODE_COUNT][NODE_COUNT];		// TODO: unsigned short
order_entry_t orders[ORDER_COUNT];
char city[ORDER_COUNT][LINE_LENGTH];

typedef struct {
	unsigned char	truck;
	unsigned char	day;
	unsigned	start;
	unsigned	cached_time;
	unsigned	cached_volume;
} loop_entry_t;

typedef struct {
	loop_entry_t	loop[LOOP_COUNT];
	unsigned	link[ORDER_COUNT];
} solution_t;

solution_t solution;
unsigned tabu_list[TABU_LENGTH] = { 60000, 60000, 60000, 60000, 60000, 60000, 60000 };
int tabu_pointer = 0;


bool tabu( int order ) {

	for ( int i = 0; i < TABU_LENGTH; ++i ) {
		if ( tabu_list[i] == order ) return true;
	}
	return false;
}

void make_tabu( int order ) {

	tabu_list[tabu_pointer] = order;
	tabu_pointer = (tabu_pointer+1)%TABU_LENGTH;

}


int main( int argc, char* argv[] ) {

	puts( "== afval ==========" );

	//read_distances_text( "AfstandenMatrix.txt" );
	read_orders_text( "orders.csv" ); // we need this for the placenames
	
	//write_distances( "distances" );
	//write_travel_time( "travel_time" );
	//write_orders( "orders" );

	read_distances( "distances" );
	read_travel_time( "travel_time" );
	read_orders( "orders" );
	
	// expeermental, somehat evil hack
	for ( int i = 0; i < NODE_COUNT; ++i ) {
		//travel_time[BASE_LOCATION][i]/=4;
		//travel_time[i][BASE_LOCATION]+=HAMMER_TIME;
	}
	
	if ( argc > 1 ) {
		read_solution( argv[1] );
	} else {
		initialize_solution();
		valuate_solution(); // FIXME
	}
	
	unsigned current_minimum = valuate_solution();
	
	for ( unsigned iteration = 0; iteration < 1000000; ++iteration ) {
		sanitize_solution();
		mutate_solution(iteration<=1126);
		
		printf( "****** %i: %i:%.2i\n", iteration,
				valuate_solution()/6000,
				(valuate_solution()%6000)/100
			);

		print_solution();
		
		char s[100];
		sprintf( s, "iter/%i.txt", iteration );
		//write_solution( s ); 
		sprintf( s, "iter/%i.bin", iteration );
		//write_state( s );
		write_state( "restart.bin" );
		
		if ( valuate_solution() < current_minimum ) {
			current_minimum = valuate_solution();
			write_solution( "afval.txt", false ); 
			write_solution( "optimal.txt", true ); 
		}		

	
	}
	
	return 0;
}

void initialize_solution() {

	for ( int i = 0; i < ORDER_COUNT-1; ++i ) {
		solution.link[i] = i+1;
	}
	solution.link[ORDER_COUNT-1] = RETURN_TO_BASE;
	
	solution.loop[FREE_LIST].day	= FREE_LIST_DAY;
	solution.loop[FREE_LIST].truck	= FREE_LIST_TRUCK;
	solution.loop[FREE_LIST].start	= 0;
	solution.loop[FREE_LIST].cached_time = SANITY_VALUE_NOORDERS/3;
	
	for( int i = 1; i < LOOP_COUNT; ++i ) {
		solution.loop[i].truck	= NO_TRUCK;
		solution.loop[i].day	= NO_DAY;
		solution.loop[i].start	= RETURN_TO_BASE;
		solution.loop[i].cached_time = HAMMER_TIME;
		solution.loop[i].cached_volume = 0;
	}
}

void sanitize_solution() {

	bool has_trivial[TRUCK_COUNT][DAY_COUNT] =
		{{false, false, false, false, false},
		 {false, false, false, false, false}};

	// make sure there is exactly one trivial loop per truck per day
	// FIXME: currently not exectly, but at least
	
	for ( int i = 1; i < LOOP_COUNT; ++i ) {
		if ( solution.loop[i].start == RETURN_TO_BASE ) {
			has_trivial[solution.loop[i].truck-1][solution.loop[i].day-1] = true;
		}
	}
	
	for ( int i = 0; i < TRUCK_COUNT; ++i ) {
		for ( int j = 0; j < DAY_COUNT; ++j ) {
			if ( !has_trivial[i][j] ) {
				for ( int k = 1; k < LOOP_COUNT; ++k ) {
					if ( solution.loop[k].truck == NO_TRUCK ) {
						solution.loop[k].truck = i+1;
						solution.loop[k].day = j+1;
						solution.loop[k].start = RETURN_TO_BASE;
						break;
					}
				}
			}
		}
	}
	
}

unsigned valuate_solution() {

//FIXME: value can be signed?
//FIXME: multi-day orders
//FIXME: violated truck capacity/time constraints

	unsigned order, from, to, value = 0;
	unsigned total_time[TRUCK_COUNT][DAY_COUNT] = {{0,0,0,0,0},{0,0,0,0,0}};
	
	// penalty for skipped orders
	/*order = solution.loop[FREE_LIST].start;
	while ( order != RETURN_TO_BASE ) {
		value += 3 * orders[order].time;
		order = solution.link[order];
	}
	
	if ( value != 3 * solution.loop[0].cached_time ) {
		printf( "CACHE INCONSISTENCY DETECTED!!! (EXPECTED: %i  FOUND: %i)\n", value, solution.loop[0].cached_time * 3 );
		dump_loop(0);
		dump_loop(1);
		exit(1);
	}*/
	value += PENALTY_NOT_PLANNED * solution.loop[0].cached_time;
	
	// time for trucks
	unsigned time, volume;
	for ( int i = 1; i < LOOP_COUNT; ++i ) {
		//if ( solution.loop[i].start == RETURN_TO_BASE ) continue;
		time = volume = 0;
		order = solution.loop[i].start;
		from = BASE_LOCATION;
		/*while ( order != RETURN_TO_BASE ) {
			to = orders[order].matrix_id;
			
			time += travel_time[from][to] + orders[order].time;
			volume += orders[order].volume;
			
			from = to;
			order = solution.link[order];
		}
		time += travel_time[from][BASE_LOCATION];
		
		if ( volume != solution.loop[i].cached_volume ) {
			printf( "CACHE INCONSISTENCY DETECTED (VOLUME)!!! (EXPECTED: %i  FOUND: %i)\n", volume, solution.loop[i].cached_volume );
			dump_loop( i );
			exit(1);
		}
			
		if ( time != solution.loop[i].cached_time ) {
			printf( "CACHE INCONSISTENCY DETECTED (TIME)!!! (EXPECTED: %i  FOUND: %i)\n", time, solution.loop[i].cached_time );
			dump_loop( i );
			exit(1);
		}*/
		volume = solution.loop[i].cached_volume;
		time = solution.loop[i].cached_time;// + HAMMER_TIME;

		total_time[solution.loop[i].truck-1][solution.loop[i].day-1] += time;
		
		if ( volume <= MAXIMUM_CAPACITY && time <= MATSECONDS_PER_DAY && solution.loop[2].cached_time + solution.loop[11].cached_time <= MATSECONDS_PER_DAY ) {
			value += time;
		} else {
			//SOME PENALTY
			value += PENALTY_OVER_CAPACITY * time;
		}
		
		
	}
	
	/*for ( int i = 0; i < TRUCK_COUNT; ++i ) {
		for ( int j = 0; j < DAY_COUNT; ++j ) {
			if ( total_time[i][j] > MATSECONDS_PER_DAY ) {
				//value += PENALTY_OVER_TIME * (total_time[i][j]-MATSECONDS_PER_DAY));
				//printf( "%i\n", value );
				//printf( "%i\n", PENALTY_OVER_TIME * (total_time[i][j]-MATSECONDS_PER_DAY) );
			}
		}
	}*/

	return value;
}

void mutate_solution( bool preheat ) {

	// try moving any order in some loop any other place in any other
	// (including self)
	
	unsigned order_i, order_j;
	unsigned *order_i_previous, *order_j_previous;
	unsigned order_i_previous_matrixid, order_j_previous_matrixid;

	unsigned min_val = UINT_MAX, min_order_i, min_order_j;
	unsigned *min_order_i_previous, *min_order_j_previous;
	unsigned min_loop_i, min_loop_j;
	
	unsigned old_time_i, old_time_j, min_cached_time_i, min_cached_time_j;
	
	for ( int i = 0; i < (preheat ? 1 : LOOP_COUNT); ++i ) {
		if ( solution.loop[i].day != NO_DAY ) {
			order_i_previous_matrixid = BASE_LOCATION;
			order_i_previous = &solution.loop[i].start;
			order_i = solution.loop[i].start;
			while ( order_i != RETURN_TO_BASE ) {
			
				if ( orders[order_i].frequency > 1 ) { // TODO
					order_i_previous_matrixid = orders[order_i].matrix_id;
					order_i_previous = &solution.link[order_i];
					order_i = solution.link[order_i];
					continue;
				}
			
				//printf ( "%4u | [%X] = %4u\t(start=%X, link=%X)\n", order_i, order_i_previous, *order_i_previous, &solution.loop[i].start, solution.link );
		
				// === remove the node
	
				if ( i == 0 ) {
					solution.loop[0].cached_time -= orders[order_i].time;
				} else {
					old_time_i	= solution.loop[i].cached_time;
				
					unsigned from	= order_i_previous_matrixid;
					unsigned me	= orders[order_i].matrix_id;
//					unsigned to	= orders[solution.link[order_i]==RETURN_TO_BASE?BASE_LOCATION:solution.link[order_i]].matrix_id; // FIXME: this is sad...
					unsigned to	= solution.link[order_i]==RETURN_TO_BASE?BASE_LOCATION:orders[solution.link[order_i]].matrix_id; // FIXME: this is sad...
					
					//if ( trace ) printf( "[%i,%i,%i]\n", from, me, to );
					
					solution.loop[i].cached_time -= orders[order_i].time;
					solution.loop[i].cached_time -= travel_time[from][me];
					solution.loop[i].cached_time -= travel_time[me][to];
					solution.loop[i].cached_time += travel_time[from][to];
					
					solution.loop[i].cached_volume -= orders[order_i].volume;
				}
				
				//*order_i_previous = solution.link[order_i];
				
				//if ( trace) puts("A");
				//valuate_solution();
				//if ( trace) puts("B");
				// ===================
			
				for ( int j = (preheat ? 2 : 0); j < LOOP_COUNT; ++j ) {
					if ( i == j) continue; // FIXME :'(
					if ( i == FREE_LIST && j == FREE_LIST ) continue;
					if ( solution.loop[j].day != NO_DAY ) {
						order_j_previous_matrixid = BASE_LOCATION;
						order_j_previous = &solution.loop[j].start;
						order_j = solution.loop[j].start;
						do {
						
							// ==== insert the node	
							
							if ( j == 0 ) {
								solution.loop[0].cached_time += orders[order_i].time;
							} else {
								old_time_j = solution.loop[j].cached_time;
							
								unsigned from	= order_j_previous_matrixid;
								unsigned me	= orders[order_i].matrix_id;
								unsigned to	= order_j==RETURN_TO_BASE?BASE_LOCATION:orders[order_j].matrix_id; // FIXME: this is sad...
								
								solution.loop[j].cached_time += orders[order_i].time;
								solution.loop[j].cached_time += travel_time[from][me];
								solution.loop[j].cached_time += travel_time[me][to];
								solution.loop[j].cached_time -= travel_time[from][to];
								
								solution.loop[j].cached_volume += orders[order_i].volume;
							}
							
							// TODO: we currently do note even have to perform the update itself.... (ALSO AT THE OTHER 3 PLACES!)
							//       (unless we are doing a double check for sanity)
							//       or have a more complex vlauaiton function, so probably avoid that
							//*order_j_previous = order_i;
							//solution.link[order_i] = order_j;
														
							//valuate_solution();
							// ====================

							//if ( trace ) dump_loop( j );

							//FIXME: run a quick check first!!! full valuation as far to expensive to do in the innerloop
							unsigned val = valuate_solution();
							if ( val < min_val && order_i != order_j && solution.link[order_i] != order_j && !tabu( order_i ) ) {
								min_val = val;
								min_order_i = order_i;
								min_order_j = order_j;
								min_order_i_previous = order_i_previous;
								min_order_j_previous = order_j_previous;
								min_loop_i = i;
								min_loop_j = j;
								//printf("(%i)\n", min_val);
								//dump_loop( j );
								min_cached_time_i = solution.loop[i].cached_time;
								min_cached_time_j = solution.loop[j].cached_time;
							}
							
							// === delete the node
							
							if ( j == 0 ) { // TODO: cache from insertion
								solution.loop[0].cached_time -= orders[order_i].time;
							} else {
								solution.loop[j].cached_time = old_time_j;
								solution.loop[j].cached_volume -= orders[order_i].volume;
							}
							
							//*order_j_previous = order_j;
							
							//valuate_solution();
							// ===================
							//if ( trace ) dump_loop( j );
							
							if ( j == FREE_LIST) break;
							
							if ( order_j != RETURN_TO_BASE) {
								order_j_previous_matrixid = orders[order_j].matrix_id;
								order_j_previous = &solution.link[order_j];
								order_j = solution.link[order_j];
							}								
						} while ( order_j != RETURN_TO_BASE );
					}
				}
				
				// === restore the node
				
				if ( i == 0 ) { // TODO: cache from removal?
					solution.loop[0].cached_time += orders[order_i].time;
				} else {
					solution.loop[i].cached_time = old_time_i;					
					solution.loop[i].cached_volume += orders[order_i].volume;
				}
				
				//solution.link[order_i] = *order_i_previous;
				//*order_i_previous = order_i;
				
				//valuate_solution();
				// ====================
			
				order_i_previous_matrixid = orders[order_i].matrix_id;
				order_i_previous = &solution.link[order_i];
				order_i = solution.link[order_i];
			}			
		}
	}
	
	
	// === sanity check
		if ( min_order_i >= ORDER_COUNT ) puts( "INSANE!" );
	// ================
	
	// === do the update
	*min_order_i_previous = solution.link[min_order_i];
	
	*min_order_j_previous = min_order_i;
	solution.link[min_order_i] = min_order_j;
	
	if ( min_loop_i == 0 ) {
		solution.loop[0].cached_time -= orders[min_order_i].time;
	} else {
		solution.loop[min_loop_i].cached_time = min_cached_time_i;
		solution.loop[min_loop_i].cached_volume -= orders[min_order_i].volume;
	}
	
	if ( min_loop_j == 0 ) {
		solution.loop[0].cached_time += orders[min_order_i].time;
	} else {
		solution.loop[min_loop_j].cached_time = min_cached_time_j;
		solution.loop[min_loop_j].cached_volume += orders[min_order_i].volume;
	}
	
	valuate_solution();
	
	// =================
	
	//dump_loop( 0 );
	//dump_loop( 1 );
	printf( "I have decided to move order %u from loop %u to loop %u (just before order %u).\n", min_order_i, min_loop_i, min_loop_j, min_order_j );
	make_tabu( min_order_i );
	
	valuate_solution();

}

void print_solution() {

	unsigned order, from, to;
	unsigned count, time, volume;
	
	unsigned skipped_orders = 0, total_orders = 0;
	unsigned skipped[5] = {0,0,0,0,0};
	unsigned totals[TRUCK_COUNT][5] = {{0,0,0,0,0},{0,0,0,0,0}};
	
	order = solution.loop[FREE_LIST].start;
	while ( order != RETURN_TO_BASE ) {
		skipped_orders++;
		skipped[orders[order].frequency-1]++;
		total_orders++;
		order = solution.link[order];
	}
	
	printf( "SKIPPED=%u (1: %u  2: %u  3: %u  4: %u  5: %u)\n\n",
		skipped_orders, skipped[0], skipped[1], skipped[2], skipped[3], skipped[4] );

	for ( int i = 1; i < LOOP_COUNT; ++i ) {
		if ( solution.loop[i].day != NO_DAY ) {
			count = time = volume = 0;
			order = solution.loop[i].start;
			from = BASE_LOCATION;
			while ( order != RETURN_TO_BASE ) {
				to = orders[order].matrix_id;
				
				count++; total_orders++;
				time += travel_time[from][to] + orders[order].time;
				volume += orders[order].volume;
			
				from = to;
				order = solution.link[order];
			}
			time += travel_time[from][BASE_LOCATION];
			time += HAMMER_TIME;
			totals[solution.loop[i].truck-1][solution.loop[i].day-1] += time;
			printf( "%3i: %c%i (volume=%u/%u) (time=%i:%.2i|%i:%.2i) (orders=%u)\n",
				i,
				solution.loop[i].truck == TRUCK_A ? 'A' : 'B',
				solution.loop[i].day,
				volume,
				MAXIMUM_CAPACITY,
				time/6000,
				(time%6000)/100,
				solution.loop[i].cached_time/6000,
				(solution.loop[i].cached_time%6000)/100,
				count
			);
		}
	}
	
	for ( int i = 0; i < TRUCK_COUNT; ++i ) {
		puts( "" );
		for ( int j = 0; j < DAY_COUNT; ++j ) {
			printf( "%c%i=%3i:%.2i    ",
				i+1 == TRUCK_A ? 'A' : 'B',
				j+1,
				totals[i][j]/6000,
				(totals[i][j]%6000)/100
			);

		}
	}
	
	printf( "\n\nTOTAL=%u/%u\n", total_orders, ORDER_COUNT );
	
	if ( total_orders != ORDER_COUNT ) {
		puts( "FUCK!" );
		for ( int i = 0; i < LOOP_COUNT; ++i ) {
			if ( solution.loop[i].day != NO_DAY) dump_loop( i );
		}
		exit(1);
	}
	
}

void read_distances_text( const char* fname ) {

	unsigned from, to, dist, trav;

	char line[LINE_LENGTH];

	FILE* f = fopen( fname, "r" );

	fgets( line, LINE_LENGTH, f );
	
	for ( int i = 0; i < NODE_COUNT; ++i ) {
		for ( int j = 0; j < NODE_COUNT; ++j ) {
			distance[i][j] = travel_time[i][j] = USHRT_MAX;
		}
	}

	while ( !feof( f ) ) {
		fscanf( f, "%u;%u;%u;%u", &from, &to, &dist, &trav );
		distance[from][to] = dist;
		travel_time[from][to] = trav;
	}
	
	fclose( f );
	
}

void read_orders_text( const char* fname ) {
	
	unsigned order_id, matrix_id, volume, time;
	unsigned char frequency;
	
	char line[LINE_LENGTH];

	FILE* f = fopen( fname, "r" );

	fgets( line, LINE_LENGTH, f );

	for ( int i = 0; i < ORDER_COUNT; ++i ) {
		fscanf( f, "%u,%u,%u,%u,%u,%s", &order_id, &matrix_id,
			&frequency, &volume, &time, city[i] );
		orders[i].order_id = order_id;
		orders[i].matrix_id = matrix_id;
		orders[i].volume = volume;
		orders[i].time = time;
		orders[i].frequency = frequency;
	}
	
	fclose( f );
	
	
}

void read_distances( const char* fname ) {

	FILE* f = fopen( fname, "rb" );
	fread( distance, sizeof( unsigned short ), NODE_COUNT * NODE_COUNT, f);
	fclose( f );
}

void read_travel_time( const char* fname ) {

	FILE* f = fopen( fname, "rb" );
	fread( travel_time, sizeof( unsigned short ), NODE_COUNT * NODE_COUNT, f);
	fclose( f );
}

void read_orders( const char* fname ) {

	FILE* f = fopen( fname, "rb" );
	fread( orders, sizeof( order_entry_t ), ORDER_COUNT, f);
	fclose( f );
	
//	for (int i = ORDER_COUNT-10; i < ORDER_COUNT; ++i ) {
//	printf( "%i: %u, %u, %u, %u, %u\n", i, orders[i].order_id,
//		orders[i].matrix_id, orders[i].volume,
//		orders[i].time, orders[i].frequency );
//	}

}

void read_solution( const char* fname ) {

	FILE* f = fopen( fname, "rb" );
	fread( &solution, sizeof( solution ), 1, f);
	fclose( f );
}

void write_distances( const char* fname ) {

	FILE* f = fopen( fname, "wb" );
	fwrite( distance, sizeof( unsigned short ), NODE_COUNT * NODE_COUNT, f);
	fclose( f );
}

void write_travel_time( const char* fname ) {

	FILE* f = fopen( fname, "wb" );
	fwrite( travel_time, sizeof( unsigned short ), NODE_COUNT * NODE_COUNT, f);
	fclose( f );
}

void write_orders( const char* fname ) {

	FILE* f = fopen( fname, "wb" );
	if ( fwrite( orders, sizeof( order_entry_t ), ORDER_COUNT, f) != sizeof( order_entry_t ) * ORDER_COUNT ) {
		puts( "FAIL!" );
	}
	fclose( f );
}

void write_solution( const char* fname, bool placenames ) {

	FILE* f = fopen( fname, "w" );
	
	for ( int i = 1; i < LOOP_COUNT; ++i ) {
		unsigned order = solution.loop[i].start;
		unsigned j = 1;
		while ( order != RETURN_TO_BASE ) {
			if ( placenames ) {
				fprintf( f, "%u;%u;%u;%u;%s\n",
				solution.loop[i].truck, solution.loop[i].day,
				j, orders[order].order_id, city[order] );
			} else {
				fprintf( f, "%u;%u;%u;%u\n",
				solution.loop[i].truck, solution.loop[i].day,
				j, orders[order].order_id );
			}
			order = solution.link[order];
			++j;
		}
		if ( j > 1 ) fprintf( f, "%u;%u;%u;%u\n",
			solution.loop[i].truck, solution.loop[i].day, j, 0 );
	}
	
	fclose( f );
}

void write_state( const char* fname ) {
	FILE* f = fopen( fname, "wb" );
	fwrite( &solution, sizeof( solution_t ), 1, f);
	fclose( f );
}

void dump_loop( int i ) {

	unsigned order = solution.loop[i].start;

	printf( "%i[%i]: START --(%i)-> ", i, solution.loop[i].cached_time, travel_time[BASE_LOCATION][order==RETURN_TO_BASE?BASE_LOCATION:orders[order].matrix_id] );

	while( order != RETURN_TO_BASE ) {
		printf( "%u (%i) --(%i)-> ", order, orders[order].time,
			travel_time[orders[order].matrix_id][solution.link[order]==RETURN_TO_BASE?BASE_LOCATION:orders[solution.link[order]].matrix_id] );
		order = solution.link[order];
	}
	printf( "RETURN_TO_BASE\n" );
}

