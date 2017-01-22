#include <iostream>
#include <fstream>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include <map>
#include <algorithm>
#include "sep_writer.h"
#include "sep_reader.h"
#include "viewer.h"
#include "producer_consumer.h"
#include "networking.h"

long total_ind_x = 6;//14;
long total_ind_y = 6;//14;
long total_ind_z = 6;//14;

long distributed_index;

Server server;

template < typename T , int N = 3 , int S = 4096 >
struct Edge
{
    long pos[N]; // +++ corner ; local
    long neg[N]; // --- corner ; local
    long pos_out[N]; // +++ corner ; local
    long neg_out[N]; // --- corner ; local
    long size[N];
    long target_index[N];
    T data_1[S];
    Edge () {}
    Edge ( long * _pos_in // +++ corner ; local
         , long * _neg_in // --- corner ; local
         , long * _pos_out// +++ corner ; local
         , long * _neg_out// --- corner ; local
         , long * _size
         , long * _target_index
         , T * _data_1
         )
    {
        long total_size = 1;
        for ( int k(0)
            ; k < N
            ; ++k
            )
        {
            total_size *= _pos_in[k] - _neg_in[k];
            size[k] = _size[k];
            pos[k] = _pos_in[k];
            neg[k] = _neg_in[k];
            pos_out[k] = _pos_out[k];
            neg_out[k] = _neg_out[k];
            target_index[k] = _target_index[k];
        }
        if ( total_size <= 0 )
        {
            std::cout << "edge boundaries error." << std::endl;
            exit(1);
        }
        update ( _data_1 );
    }
    void update ( T * _data_1 )
    {
        for ( int x(neg[0])
            , k(0)
            ; x < pos[0]
            ; ++x
            )
        {
            for ( int y(neg[1])
                , ind2
                ; y < pos[1]
                ; ++y
                )
            {
                ind2 = size[2] * ( y + size[1] * x );
                for ( int z(neg[2])
                    ; z < pos[2]
                    ; ++z
                    , ++k
                    )
                {
                    data_1[k] = _data_1[ind2+z];
                }
            }
        }
    }
    void apply ( T * _data_1 )
    {
        for ( int x(neg_out[0])
            , k(0)
            ; x < pos_out[0]
            ; ++x
            )
        {
            for ( int y(neg_out[1])
                , ind2
                ; y < pos_out[1]
                ; ++y
                )
            {
                ind2 = size[2] * ( y + size[1] * x );
                for ( int z(neg_out[2])
                    ; z < pos_out[2]
                    ; ++z
                    , ++k
                    )
                {
                    _data_1[ind2+z] = data_1[k];
                }
            }
        }
    }
};

long distributed_tile[3];

long num_tile = 2;

template < int N = 3 >
long resolve_distributed_tile ( long * index , long * tile , long num_tile )
{
    long ind = 0;
    long fact = 1;
    for ( int k(0)
        ; k < N
        ; ++k
        , fact *= num_tile
        )
    {
        ind += fact * ( index[k] / tile[k] );
    }
    return ind;
}

struct NetworkNode
{
    std::string host;
    long port;
    Client client;
    ProducerConsumer < Edge < float , 3 , 4096 > * > * edge_queue;
};

ProducerConsumer < Edge < float , 3 , 4096 > * > * input_edge_queue;

ProducerConsumer < int > num_network_requests(100000);

std::vector < NetworkNode > network_list;

template < typename T , int N , int S >
void Send ( Edge < T , N , S > * e )
{
    long ind ( resolve_distributed_tile ( e -> target_index , distributed_tile , num_tile ) );
    network_list[ind] . edge_queue -> produce ( e );
    num_network_requests . produce ( ind );
}

Message & operator << ( Message & m , Edge<float> const & e )
{
    long ind = 0;
    long chunk = 0;
    long * tmp;
    float * tmp_f;
    Chunk c;
    m . data . push_back ( c );
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos[2]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg[2]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos_out[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos_out[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . pos_out[2]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg_out[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg_out[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . neg_out[2]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . size[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . size[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . size[2]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . target_index[0]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . target_index[1]; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    *tmp = e . target_index[2]; ind += 4;
    for ( int k(0)
        , total_size ( e . size[0] * e . size[1] * e . size[2] )
        ; k < total_size
        ; ++k
        )
    {
        tmp_f = reinterpret_cast < float * > ( m . data[chunk] . buffer[ind] );
        *tmp_f = e . data_1[k]; ind += 4; if ( ind % 256 == 0 ) { ind = 0; chunk++; Chunk c; m . data . push_back ( c ); }
    }
    return m;
}

Message & operator >> ( Message & m , Edge<float> & e )
{
    long ind = 0;
    long chunk = 0;
    long * tmp;
    float * tmp_f;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos[2] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg[2] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos_out[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos_out[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . pos_out[2] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg_out[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg_out[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . neg_out[2] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . size[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . size[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . size[2] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . target_index[0] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . target_index[1] = *tmp; ind += 4;
    tmp = reinterpret_cast < long * > ( m . data[chunk] . buffer[ind] );
    e . target_index[2] = *tmp; ind += 4;
    for ( int k(0)
        , total_size ( e . size[0] * e . size[1] * e . size[2] )
        ; k < total_size
        ; ++k
        )
    {
        tmp_f = reinterpret_cast < float * > ( m . data[chunk] . buffer[ind] );
        e . data_1[k] = *tmp_f; ind += 4; if ( ind % 256 == 0 ) { ind = 0; chunk++; }
    }
    return m;
}

template < typename T , int N , long size_x , long size_y , long size_z , long len >
struct WavePropagationTile;


std::vector      < WavePropagationTile<float,3,64,64,64,262144> * > propagation_vect ;

template < typename T , int N = 3 , long size_x = 64 , long size_y = 64 , long size_z = 64 , long len = 262144 >
struct WavePropagationTile
{
    T * data_1;
    T * data_2;
    T * vel_2;
    long long boundary;
    std::string file_name;
    long pos[N]; // +++ corner ; global
    long neg[N]; // --- corner ; global
    float scale[N];
    float h;
    long index[N];
    Edge<T,N> * side;
    Edge<T,N> * update_side;
    bool data_present[6];
    long num_sides;
    WavePropagationTile<T,N,size_x,size_y,size_z,len> ** neighbors;
    long long timer;
    WavePropagationTile( float _h
                       , float * _scale
                       , long * _index
                       , long * _boundary_index
                       )
    {
        timer = 0;
        h = _h;
        data_1 = new T[len];
        data_2 = new T[len];
        vel_2  = new T[len];
        for ( int k(0)
            ; k < N
            ; ++k
            )
        {
            scale[k] = _scale[k];
            index[k] = _index[k];
        }
        {
            side = new Edge<T,N>[2*N];
            update_side = new Edge<T,N>[2*N];

            for ( int k(0)
                ; k < N
                ; ++k
                )
            {
                data_present[2*k  ] = index[k]!=0;
                data_present[2*k+1] = index[k]!=_boundary_index[k]-1;
                long size[N];
                size[0] = size_x;
                size[1] = size_y;
                size[2] = size_z;
                long _neg[N];
                long _pos[N];
                long _neg_out[N];
                long _pos_out[N];
                long _target_index[N];
                if ( data_present[2*k] )
                {
                    _neg[0] = 1;
                    _neg[1] = 1;
                    _neg[2] = 1;
                    _pos[0] = size_x-1;
                    _pos[1] = size_y-1;
                    _pos[2] = size_z-1;
                    _pos[k] = 2;
                    _neg_out[0] = 1;
                    _neg_out[1] = 1;
                    _neg_out[2] = 1;
                    _pos_out[0] = size_x-1;
                    _pos_out[1] = size_y-1;
                    _pos_out[2] = size_z-1;
                    _neg_out[k] = size[k]-1;
                    _pos_out[k] = size[k];
                    _target_index[0] = index[0];
                    _target_index[1] = index[1];
                    _target_index[2] = index[2];
                    _target_index[k]--;
                    side[2*k] = Edge<T,N> ( _pos
                                          , _neg
                                          , _pos_out
                                          , _neg_out
                                          , size
                                          , _target_index
                                          , data_1
                                          );
                }
                if ( data_present[2*k+1] )
                {
                    _neg[0] = 1;
                    _neg[1] = 1;
                    _neg[2] = 1;
                    _pos[0] = size_x-1;
                    _pos[1] = size_y-1;
                    _pos[2] = size_z-1;
                    _neg[k] = size[k]-2;
                    _neg_out[0] = 1;
                    _neg_out[1] = 1;
                    _neg_out[2] = 1;
                    _pos_out[0] = size_x-1;
                    _pos_out[1] = size_y-1;
                    _pos_out[2] = size_z-1;
                    _neg_out[k] = 0;
                    _pos_out[k] = 1;
                    _target_index[0] = index[0];
                    _target_index[1] = index[1];
                    _target_index[2] = index[2];
                    _target_index[k]++;
                    if ( _target_index[k] < 0 )
                    {
                        std::cout << "index error" << std::endl;
                        exit(1);
                    }
                    side[2*k+1] = Edge<T,N> ( _pos
                                            , _neg
                                            , _pos_out
                                            , _neg_out
                                            , size
                                            , _target_index
                                            , data_1
                                            );
                }
            }

        }
        num_sides = 0;
        for ( int k(0)
            ; k < 6
            ; ++k
            )
        {
            if ( data_present[k] )
            {
                num_sides ++;
            }
        }
    }
    void propagate()
    {
        load();
        timer++;
        float fact ( h * h * h / ( scale[0] * scale[1] * scale[2] ) );
        long size_2 = size_y*size_z;
        for ( long x(1)
            ; x+1 < size_x
            ; ++x
            )
        {
            for ( long y(1) , ind_pref
                ; y+1 < size_y
                ; ++y
                )
            {
                ind_pref = size_z*(y+size_y*x);
                for ( long z(1) , ind
                    ; z+1 < size_z
                    ; ++z
                    )
                {
                    ind = ind_pref + z;
                    data_2[ind] = fact * vel_2[ind] * ( -6*data_1[ind] 
                                                    +      data_1[ind+1]
                                                    +      data_1[ind-1]
                                                    +      data_1[ind+size_z]
                                                    +      data_1[ind-size_z]
                                                    +      data_1[ind+size_2]
                                                    +      data_1[ind-size_2]
                                                      )
                                 + 2 * data_1[ind]
                                 -     data_2[ind]
                                 ;
                }
            }
        }
        T tmp;
        for ( long k(0)
            ; k < len
            ; ++k
            )
        {
            tmp = data_1[k];
            data_1[k] = data_2[k];
            data_2[k] = tmp;
        }
        update_edges();
    }
    void load()
    {
        for ( int k(0)
            ; k < 6
            ; ++k
            )
        {
            if ( data_present[k] )
            {
                update_side[k] . apply ( data_1 );
            }
        }
    }
    void send_edges()
    {
        for ( int k(0)
            , j
            ; k < 6
            ; ++k
            )
        {
            if ( data_present[k] )
            {
                if ( neighbors[k] )
                {
                    j = (k%2==1)?k-1:k+1;
                    neighbors[k] -> update_side[j] = side[k];
                }
                else
                {
                    // send update via distributed network
                    Send(&side[k]);
                }
            }    
        }

    }
    void update_edges()
    {
        for ( long k(0)
            ; k < 6
            ; ++k
            )
        {
            if ( data_present[k] )
            {
                side[k] . update ( data_1 );
                side[k] . update ( data_1 );
            }
        }

    }
};

void propagate_worker ( int k , int n )
{
    for ( int i(k) ; i < propagation_vect . size () ; i+=n )
    if ( propagation_vect [i] )
    {
        propagation_vect [i] -> propagate ( ) ;
    }
}

void send_edges_worker ( int k , int n )
{
    for ( int i(k) ; i < propagation_vect . size () ; i+=n )
    if ( propagation_vect [i] )
    {
        propagation_vect [i] -> send_edges ( ) ;
    }
}

boost::mutex mutex_;
boost::condition_variable condition;

void recv_edges_worker ( )
{
    while ( 1 )
    {
        //usleep(200);
        //std::cout << "recv edge worker : " << input_edge_queue -> size () << std::endl;
        /*Edge * e =*/ input_edge_queue -> consume ();
        //neighbors[0] -> update_side[0] = side[0];
        //if ( num_network_requests . size () )
        num_network_requests . consume (  );
        //if ( num_network_requests . size () )
        //num_network_requests . consume (  );
        //std::cout << "new size : " << distributed_index << "   " << input_edge_queue -> size () << std::endl;
        std::cout << "net size : " << distributed_index << "   " << num_network_requests . size () << std::endl;
        if ( /*input_edge_queue -> size () == 0 &&*/ num_network_requests . size () == 0 )
        {
            //std::cout << "condition notify" << std::endl;
            condition . notify_one ();
        }
    }
}

float get_argument ( int &   arg 
                   , int     argc
                   , char ** argv
                   )
{
    ++arg;
    if ( arg > argc )
    {
        //std::cout << "parameter error" << std::endl;
        exit(1);
    }
    return atof ( argv[arg] );
}

template < typename T >
inline void mem_cpy ( T * out , T * in , int num )
{
    for ( int k(0)
        ; k < num
        ; ++k
        )
    {
        out[k] = in[k];
    }
}

void propagate ( float   h       // dt
               , int     num_x 
               , int     num_t
               , float   scale_x
               , float   scale_t
               , float * p2 
               , float * p1 
               , float * c 
               , float * vel )
{
    float fact ( h * h / ( scale_x * scale_x ) );
    for ( int t(1)
        ; t+1 < num_t
        ; ++t
        )
    {
        for ( int x(1)
            ; x+1 < num_x
            ; ++x
            )
        {
            if ( fabs ( p2[t+x*num_t] ) > 1e-40 
              || fabs ( p1[t+1+x*num_t] ) > 1e-40 
              || fabs ( p1[t-1+x*num_t] ) > 1e-40 
              || fabs ( p1[t+(x+1)*num_t] ) > 1e-40 
              || fabs ( p1[t+(x-1)*num_t] ) > 1e-40 
               )
            {
                c[t+x*num_t] = fact * vel[t+x*num_t] * vel[t+x*num_t] * ( -4*p1[t+x*num_t] 
                                                                        +    p1[t+(x+1)*num_t]
                                                                        +    p1[t+(x-1)*num_t]
                                                                        +    p1[(t+1)+x*num_t]
                                                                        +    p1[(t-1)+x*num_t]
                                                                        )
                             + 2 * p1[t+x*num_t]
                             -     p2[t+x*num_t]
                             ;
            }
        }
    }
    for ( int k(0)
        ; k < num_t * num_x
        ; ++k
        )
    {
        p2[k] = p1[k];
    }
    for ( int k(0)
        ; k < num_t * num_x
        ; ++k
        )
    {
        p1[k] = c[k];
    }
}

float find_max ( float const * x , int num )
{
    float m ( 0 ) , t;
    for ( int k(0)
        ; k < num
        ; ++k
        )
    {
        t = fabs(x[k]);
        if ( t > m )
        {
            m = t;
        }
    }
    return m;
}

float find_min ( float const * x , int num )
{
    float m ( 1e10 ) , t;
    for ( int k(0)
        ; k < num
        ; ++k
        )
    {
        t = fabs(x[k]);
        if ( t < m )
        {
            m = t;
        }
    }
    return m;
}

struct source
{

    float pos_x; // m
    float pos_z; // m

    source ( int _pos_x , int _pos_z )
    : pos_x ( _pos_x )
    , pos_z ( _pos_z )
    {

    }

};

struct recorder
{

    float pos_x; // m
    float pos_z; // m

    recorder ( int _pos_x , int _pos_z )
    : pos_x ( _pos_x )
    , pos_z ( _pos_z )
    {

    }

};

void calculate_poyinting_vector ( vect        * p 
                                , float const * u
                                , int           num_x
                                , int           num_z
                                )
{
    int disp = 1;
    for ( int x(disp)
        ; x+disp < num_x
        ; ++x
        )
    {
        for ( int z(disp)
            ; z+disp < num_z
            ; ++z
            )
        {
            p[z+x*num_z].vx += (u[z+(x+disp)*num_z] - u[z+(x-disp)*num_z]);
            p[z+x*num_z].vz += (u[z+disp  +x*num_z] - u[z-disp+  x*num_z]);
            p[z+x*num_z].vy = 0;
        }
    }
}

void parse_network_topology ( std::string & file_name )
{
    std::string line;
    std::ifstream file;
    file . open ( file_name );
    if ( file . is_open () )
    {
        while ( ! file . eof () )
        {
            getline ( file , line );
            std::cout << line << std::endl;
            std::stringstream ss ( line );
            int index;
            ss >> index;
            std::string host;
            ss >> host;
            long port;
            ss >> port;
            if ( host != "" )
            {
                NetworkNode node;
                node . host = host;
                node . port = port;
                node . edge_queue = new ProducerConsumer < Edge < float , 3 , 4096 > * > ( 100000 );
                std::cout << "host:" << node . host << std::endl;
                std::cout << "port:" << node . port << std::endl;
                network_list . push_back ( node );
            }
        }
    }
}

void create_server ( Server & s , int distributed_index )
{
    std::cout << "create server : size : " << network_list . size () << std::endl;
    for ( int k(0)
        ; k < 100
        ; ++k
        )
    s . Create ( network_list[distributed_index] . port , network_list . size () - 1 );
}

void manage_server ( Server & s , int distributed_index , int k )
{

    while ( 1 )
    {
        Message package;
        s . Receive ( package , k );
        Edge < float , 3 , 4096 > e;
        input_edge_queue -> produce ( &e );
    }
    
}

void manage_client ( NetworkNode & n , int k )
{
    std::cout << "connecting to:" << n . host << " " << n . port << std::endl;
    n . client . Connect ( n . host 
                         , n . port
                         );
    sleep(3);
    while ( 1 )
    {
        n . edge_queue -> consume ();
        Message package;
        Test t;
        package << t;
        n . client . Send ( package );
    }
}

int main( int argc , char ** argv )
{

    if ( argc != 3 )
    {
        std::cout << "please specify node index" << std::endl;
        exit(1);
    }

    distributed_index = atoi ( argv[1] );
    std::cout << "pass 1" << std::endl;

    std::string network_topology ( argv[2] );
    std::cout << "pass 1.1" << std::endl;

    parse_network_topology ( network_topology );
    std::cout << "pass 1.2" << std::endl;

    input_edge_queue = new ProducerConsumer < Edge < float , 3 , 4096 > * > ( 100000 );
    boost::thread * server_thread = new boost::thread ( create_server , server , distributed_index );
    std::cout << "pass 1.3" << std::endl;
    std::cout << "pass 1.5" << std::endl;
    std::cout << "pass 1.6" << std::endl;

    std::cout << "pass 1.7" << std::endl;

    for ( int k(0)
        ; k < network_list . size ()
        ; ++k
        )
    {
        if ( distributed_index == k )
        {

        }
        else
        {
            boost::thread * client_thread = new boost::thread ( manage_client , network_list[k] , k );
        }
        sleep(1);
    }
    sleep(3);
    std::cout << "pass 1.8" << std::endl;
    for ( int k(0)
        ; k < network_list . size () - 1
        ; ++k
        )
    boost::thread * server_thread = new boost::thread ( manage_server , server , distributed_index , k );

    std::cout << "pass 1.4" << std::endl;
    boost::thread * recv_edges_thread = new boost::thread ( recv_edges_worker );

    distributed_tile[0] = total_ind_x/2;
    distributed_tile[1] = total_ind_y/2;
    distributed_tile[2] = total_ind_z;

    float scale[3];
    scale[0] = 25;
    scale[1] = 25;
    scale[2] = 25;
    WavePropagationTile<float,3,64,64,64,262144> **** neigh;
    neigh = new
    WavePropagationTile<float,3,64,64,64,262144> *** [total_ind_x];
    for ( long x(0)
        ; x < total_ind_x
        ; ++x
        )
    {
    neigh[x] = new
    WavePropagationTile<float,3,64,64,64,262144> ** [total_ind_y];
    for ( long y(0)
        ; y < total_ind_y
        ; ++y
        )
    {
    neigh[x][y] = new
    WavePropagationTile<float,3,64,64,64,262144> * [total_ind_z];
    for ( long z(0)
        ; z < total_ind_z
        ; ++z
        )
    {
        long index[3];
        index[0] = x;
        index[1] = y;
        index[2] = z;
        long total_index[3];
        total_index[0] = total_ind_x;
        total_index[1] = total_ind_y;
        total_index[2] = total_ind_z;

        WavePropagationTile<float,3,64,64,64,262144> * v = NULL;

        if ( resolve_distributed_tile ( index , distributed_tile , num_tile ) == distributed_index )
        v =
        new WavePropagationTile<float,3,64,64,64,262144> ( 1e-5
                                                         , scale
                                                         , index
                                                         , total_index
                                                         );

        neigh[x][y][z] = v;
        propagation_vect  . push_back ( v );
    }
    }
    }
    std::cout << "pass 2" << std::endl;
    for ( long x(0)
        ; x < total_ind_x
        ; ++x
        )
    {
    for ( long y(0)
        ; y < total_ind_y
        ; ++y
        )
    {
    for ( long z(0)
        ; z < total_ind_z
        ; ++z
        )
    {
        WavePropagationTile<float,3,64,64,64,262144> ** n = new WavePropagationTile<float,3,64,64,64,262144> * [6];
        if ( x == 0 )
        {
            n[0] = NULL;
        }
        else
        {
            n[0] = neigh[x-1][y][z];
        }
        if ( x == total_ind_x-1 )
        {
            n[1] = NULL;
        }
        else
        {
            n[1] = neigh[x+1][y][z];
        }
        if ( y == 0 )
        {
            n[2] = NULL;
        }
        else
        {
            n[2] = neigh[x][y-1][z];
        }
        if ( y == total_ind_y-1 )
        {
            n[3] = NULL;
        }
        else
        {
            n[3] = neigh[x][y+1][z];
        }
        if ( z == 0 )
        {
            n[4] = NULL;
        }
        else
        {
            n[4] = neigh[x][y][z-1];
        }
        if ( z == total_ind_z-1 )
        {
            n[5] = NULL;
        }
        else
        {
            n[5] = neigh[x][y][z+1];
        }
        if ( neigh[x][y][z] )
        neigh[x][y][z] -> neighbors = n;
    }
    }
    }
    std::cout << "pass 3" << std::endl;
    
    int n_thread = 4;
    for ( int it_k(0) ; it_k < 200 ; ++it_k  )
    {
        std::cout << it_k << std::endl;
        std::vector < boost::thread * > thread_th;
        for ( int k(0)
            ; k < n_thread
            ; ++k
            )
        {
            boost::thread * thr1 = new boost::thread ( propagate_worker , k , n_thread );
            thread_th . push_back ( thr1 );
        }
        for ( int k(0)
            ; k < thread_th.size()
            ; ++k
            )
        {
            thread_th[k] -> join ();
            delete thread_th[k];
        }
        std::vector < boost::thread * > thread_send_th;
        for ( int k(0)
            ; k < n_thread
            ; ++k
            )
        {
            boost::thread * thr1 = new boost::thread ( send_edges_worker , k , n_thread );
            thread_send_th . push_back ( thr1 );
        }
        for ( int k(0)
            ; k < thread_send_th.size()
            ; ++k
            )
        {
            thread_send_th[k] -> join ();
            delete thread_send_th[k];
        }
        //std::cout << "final_size = " << ( num_network_requests . size () ) << std::endl;
        //usleep(1000000);
        boost::unique_lock<boost::mutex> lock(mutex_);
        //std::cout << "wait" << std::endl;
        condition . wait (lock);
        //std::cout << "continue : " << distributed_index << std::endl;
    }

    std::cout << "exit" << std::endl;

    return 0;

}

