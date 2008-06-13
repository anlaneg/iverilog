/*
 *  VHDL code generator for Icarus Verilog.
 *
 *  Copyright (C) 2008  Nick Gasson (nick@nickg.me.uk)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "vhdl_target.h"
#include "vhdl_element.hh"

#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <list>
#include <map>

/*
 * Maps a signal to the entity it is defined within. Also
 * provides a mechanism for renaming signals -- i.e. when
 * an output has the same name as register: valid in Verilog
 * but not in VHDL, so two separate signals need to be
 * defined. 
 */
struct signal_defn_t {
   std::string renamed;    // The name of the VHDL signal
   const vhdl_entity *ent; // The entity where it is defined
};

typedef std::map<ivl_signal_t, signal_defn_t> signal_defn_map_t;


static int g_errors = 0;  // Total number of errors encountered
static entity_list_t g_entities;  // All entities to emit
static signal_defn_map_t g_known_signals;


/*
 * Called when an unrecoverable problem is encountered.
 */
void error(const char *fmt, ...)
{
   std::va_list args;

   va_start(args, fmt);
   std::printf("VHDL conversion error: ");  // Source/line number?
   std::vprintf(fmt, args);
   std::putchar('\n');
   va_end(args);

   g_errors++;
}

/*
 * Find an entity given a type name.
 */
vhdl_entity *find_entity(const std::string &tname)
{
   entity_list_t::const_iterator it;
   for (it = g_entities.begin(); it != g_entities.end(); ++it) {
      if ((*it)->get_name() == tname)
         return *it;
   }
   return NULL;
}

/*
 * Add an entity/architecture pair to the list of entities
 * to emit.
 */
void remember_entity(vhdl_entity* ent)
{
   assert(find_entity(ent->get_name()) == NULL);
   g_entities.push_back(ent);
}

/*
 * Remeber the association of signal to entity.
 */
void remember_signal(ivl_signal_t sig, const vhdl_entity *ent)
{
   assert(g_known_signals.find(sig) == g_known_signals.end());
   
   signal_defn_t defn = { ivl_signal_basename(sig), ent };
   g_known_signals[sig] = defn;
}

/*
 * Change the VHDL name of a Verilog signal.
 */
void rename_signal(ivl_signal_t sig, const std::string &renamed)
{
   assert(g_known_signals.find(sig) != g_known_signals.end());

   g_known_signals[sig].renamed = renamed;
}

const vhdl_entity *find_entity_for_signal(ivl_signal_t sig)
{
   assert(g_known_signals.find(sig) != g_known_signals.end());

   return g_known_signals[sig].ent;
}

const std::string &get_renamed_signal(ivl_signal_t sig)
{
   assert(g_known_signals.find(sig) != g_known_signals.end());

   return g_known_signals[sig].renamed;
}


extern "C" int target_design(ivl_design_t des)
{
   ivl_scope_t *roots;
   unsigned int nroots;
   ivl_design_roots(des, &roots, &nroots);

   for (unsigned int i = 0; i < nroots; i++)
      draw_scope(roots[i], NULL);

   ivl_design_process(des, draw_process, NULL);

   // Write the generated elements to the output file
   // only if there are no errors
   if (0 == g_errors) {
      const char *ofname = ivl_design_flag(des, "-o");
      std::ofstream outfile(ofname);
      
      for (entity_list_t::iterator it = g_entities.begin();
           it != g_entities.end();
           ++it)
         (*it)->emit(outfile);
      
      outfile.close();
   }
   
   // Clean up
   for (entity_list_t::iterator it = g_entities.begin();
        it != g_entities.end();
        ++it)
      delete (*it);
   g_entities.clear();
   
   return g_errors;
}