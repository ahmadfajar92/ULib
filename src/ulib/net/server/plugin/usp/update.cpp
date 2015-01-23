// update.cpp - dynamic page translation (update.usp => update.cpp)
   
#include <ulib/net/server/usp_macro.h>
   
#include "world.h"
   
   #ifndef AS_cpoll_cppsp_DO
   static UValue* pvalue;
   #endif
   static UOrmSession*     psql_update;
   static UOrmStatement*   pstmt1;
   static UOrmStatement*   pstmt2;
   static World*           pworld_update;
   static UVector<World*>* pvworld_update;
   
   static void usp_init_update()
   {
      U_TRACE(5, "::usp_init_update()")
   
      pworld_update  = U_NEW(World);
      pvworld_update = U_NEW(UVector<World*>(500));
   
   #ifndef AS_cpoll_cppsp_DO
      pvalue = U_NEW(UValue(ARRAY_VALUE));
   #endif
   }
   
   static void usp_fork_update()
   {
      U_TRACE(5, "::usp_fork_update()")
   
      psql_update = U_NEW(UOrmSession(U_CONSTANT_TO_PARAM("hello_world")));
   
      pstmt1 = U_NEW(UOrmStatement(*psql_update, U_CONSTANT_TO_PARAM("SELECT randomNumber FROM World WHERE id = ?")));
      pstmt2 = U_NEW(UOrmStatement(*psql_update, U_CONSTANT_TO_PARAM("UPDATE World SET randomNumber = ? WHERE id = ?")));
   
      if (pstmt1 == 0 ||
          pstmt2 == 0)
         {
         U_ERROR("usp_fork_update(): we cound't connect to db");
         }
   
      pstmt1->use( pworld_update->id);
      pstmt1->into(pworld_update->randomNumber);
      pstmt2->use( pworld_update->randomNumber, pworld_update->id);
   }
   
   static void usp_end_update()
   {
      U_TRACE(5, "::usp_end_update()")
   
      delete pstmt1;
      delete pstmt2;
      delete psql_update;
      delete pvworld_update;
      delete pworld_update;
   #ifndef AS_cpoll_cppsp_DO
      delete pvalue;
   #endif
   }  
   
extern "C" {
extern U_EXPORT int runDynamicPage_update(UClientImage_Base* client_image);
       U_EXPORT int runDynamicPage_update(UClientImage_Base* client_image)
{
   U_TRACE(0, "::runDynamicPage_update(%p)", client_image)
   
   
   // ------------------------------
   // special argument value:
   // ------------------------------
   //  0 -> call it as service
   // -1 -> init
   // -2 -> reset
   // -3 -> destroy
   // -4 -> call it for sigHUP
   // -5 -> call it after fork
   // ------------------------------
   
   if (client_image)
      {
      if (client_image == (void*)-1) { usp_init_update(); U_RETURN(0); }
   
      if (client_image == (void*)-3) { usp_end_update(); U_RETURN(0); }
   
      if (client_image == (void*)-5) { usp_fork_update(); U_RETURN(0); }
   
      if (client_image >= (void*)-5) U_RETURN(0);
   
      (void) UClientImage_Base::wbuffer->append(
         U_CONSTANT_TO_PARAM("Content-Type: application/json; charset=UTF-8\r\n\r\n"));
   
      u_http_info.endHeader = UClientImage_Base::wbuffer->size();
   
      (void) UHTTP::processForm();
      }
      
   UString queries = USP_FORM_VALUE(0);
   
   int i = 0, num_queries = queries.strtol();
   
        if (num_queries <   1) num_queries = 1;
   else if (num_queries > 500) num_queries = 500;
   
   #ifdef AS_cpoll_cppsp_DO
   USP_PUTS_CHAR('[');
   #endif
   
   while (true)
      {
      pworld_update->id = u_get_num_random(10000);
   
      pstmt1->execute();
   
      U_INTERNAL_DUMP("pworld_update->randomNumber = %u", pworld_update->randomNumber)
   
      pworld_update->randomNumber = u_get_num_random(10000);
   
      pstmt2->execute();
   
   #ifdef AS_cpoll_cppsp_DO
      USP_PRINTF("{\"id\":%u,\"randomNumber\":%u}", pworld_update->id, pworld_update->randomNumber);
   #endif
   
      pvworld_update->push_back(U_NEW(World(*pworld_update)));
   
      if (++i == num_queries) break;
   
   #ifdef AS_cpoll_cppsp_DO
      USP_PUTS_CHAR(',');
   #endif
      }
   
   #ifdef AS_cpoll_cppsp_DO
   USP_PUTS_CHAR(']');
   #else
   USP_JSON_stringify(*pvalue, UVector<World*>, *pvworld_update);
   #endif
   
   pvworld_update->clear();
   
      U_http_content_type_len = 1;
   
      UClientImage_Base::setRequestNoCache();
   
   U_RETURN(200);
} }