#ifndef _4D518F94_8181_4786_BCC3_6FFC894BDF2C
#define _4D518F94_8181_4786_BCC3_6FFC894BDF2C

#include <upcxx/backend.hpp>
#include <upcxx/completion.hpp>
#include <upcxx/future.hpp>
#include <upcxx/dist_object.hpp>
#include <upcxx/rpc.hpp>

#include <list>
#include <algorithm>

namespace upcxx {

	template <typename T>
	future<T> broadcast( T&& obj, intrank_t sender/*, team & team = world()*/ )	{
    intrank_t np = /*team.*/rank_n();
    intrank_t me = /*team.*/rank_me();

    dist_object< promise<T> > dprom( std::move( promise<T>() ) );
    promise<T> & lprom = *dprom;    

    if ( me == sender ){
      dprom->fulfill_result(obj);
      future<> fchain = make_future();
      for( int peer = 0; peer < np; peer++){
        if(peer!=sender){
                future<> f = rpc(peer, [](dist_object< promise<T> > & srcprom, T& data ) {  srcprom->fulfill_result(data); }, dprom, obj);
                fchain = when_all(fchain, f);
        }
      }
      return when_all(dprom->get_future(),fchain);
    }
  
    wait(lprom.get_future());
    return lprom.get_future();
  }

	template <typename T,typename BinaryOp>
	future<T> allreduce( T&& value, BinaryOp &&op /*, team & team = world()*/ )	{
    intrank_t root = 0;
    dist_object< promise<T> > dprom( std::move(promise<T>() ) );
    dist_object< T > dval( std::forward<T>(value) );
    //*dval = value;

    if(rank_me()==root){
      for( int peer = 0; peer < rank_n(); peer++){
        if(peer!=root){
          future<T> fut = rpc(peer,[](dist_object< promise<T> > & srcprom, dist_object< T > & srcval){ srcprom->fulfill_result(1); return *srcval;}, dprom, dval );
          value = op(value, wait(fut));
        }
      }
      dprom->fulfill_result(value);
    }

    wait(dprom->get_future());

    return broadcast( std::forward<T>(value), root ); 
  }
}

#endif
