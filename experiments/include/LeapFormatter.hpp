// Copyright (c) 2017-2022, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// [CLI11:public_includes:set]
#include <algorithm>
#include <string>
#include <vector>
// [CLI11:public_includes:end]

// #include "App.hpp"

namespace CLI {
// [CLI11:leap_formatter_hpp:verbatim]


class LeapFormatter : public Formatter {
   const char* tree_line = u8"";
   const char* tree_angle = u8"";
   const char* tree_fork = u8"";

public:
   LeapFormatter() : Formatter() {
      // this gives better, more compact display
      column_width(25);
   }
   LeapFormatter(const LeapFormatter&) = default;
   LeapFormatter(LeapFormatter&&) = default;

   /// This prints out all the subcommands
   virtual std::string make_subcommands(const App* app, AppFormatMode mode) const {
      std::stringstream out;

      std::vector<const App*> subcommands = app->get_subcommands({});

      // Make a list in definition order of the groups seen
      std::vector<std::string> subcmd_groups_seen;
      for(const App* com: subcommands) {
         if(com->get_name().empty()) {
            if(!com->get_group().empty()) {
               out << make_expanded(com);
            }
            continue;
         }
         std::string group_key = com->get_group();
         if(!group_key.empty() &&
            std::find_if(subcmd_groups_seen.begin(), subcmd_groups_seen.end(), [&group_key](std::string a) {
               return detail::to_lower(a) == detail::to_lower(group_key);
            }) == subcmd_groups_seen.end())
            subcmd_groups_seen.push_back(group_key);
      }

      // For each group, filter out and print subcommands
      for(const std::string& group: subcmd_groups_seen) {
        out << "\n" << group << ":\n";
         std::vector<const App*> subcommands_group = app->get_subcommands([&group](const App* sub_app) {
            return detail::to_lower(sub_app->get_group()) == detail::to_lower(group);
         });
         for(const App* new_com: subcommands_group) {
            if(new_com->get_name().empty())
               continue;

            std::string tree_symbol = (subcommands_group.back() == new_com ? tree_angle : tree_fork);
            std::string line_symbol = (subcommands_group.back() == new_com ? "" : tree_line);
            std::string subc_symbol = "";

            const App* parent = app->get_parent();
            if(parent != nullptr) {
               std::vector<const App*> sc_group = parent->get_subcommands([&group](const App* sub_app) {
                  return detail::to_lower(sub_app->get_group()) == detail::to_lower(group);
               });
               if(sc_group.back() != app) {
                  subc_symbol = tree_line;
               }
            }

            switch(mode) {
               case AppFormatMode::All:
                  out << tree_symbol << new_com->help(new_com->get_name(), AppFormatMode::Sub);
                  out << "\n";
                  break;
               case AppFormatMode::Normal:
               case AppFormatMode::Sub:
                  out << make_subcommand(new_com);
                  break;
               default:
                  throw HorribleError("Internal error: unknown help type requested");
            }
         }
      }

      return out.str();
   }

   /// This prints out a subcommand in help-all
   virtual std::string make_expanded(const App* sub, AppFormatMode mode = AppFormatMode::Sub) const {
      std::stringstream out;
      std::string tmp;
      std::string subc_symbol = " ";
        out << sub->get_display_name(true) << "\n";
        out << make_description(sub);
        if(sub->get_name().empty() && !sub->get_aliases().empty()) {
        detail::format_aliases(out, sub->get_aliases(), column_width_ + 2);
        }
        out << make_positionals(sub);
        out << make_groups(sub, mode);
        out << make_subcommands(sub, mode);

      // Drop blank spaces
      tmp = detail::find_and_replace(out.str(), "\n\n", "\n");
      tmp = tmp.substr(0, tmp.size() - 1);// Remove the final '\n'

      //
      auto group = sub->get_parent()->get_group();
      std::vector<const App*> sc_group = sub->get_parent()->get_subcommands(
            [&group](const App* sub_app) { return detail::to_lower(sub_app->get_group()) == detail::to_lower(group); });

      if(sc_group.back() != sub) {
         subc_symbol = tree_line;
      }

      // Indent all but the first line (the name)
      return detail::find_and_replace(tmp, "\n", "\n" + subc_symbol + "  ") + "\n";
   }

   virtual std::string make_group(std::string group, bool is_positional, std::vector<const Option*> opts) const {
      std::stringstream out;

      out << "\n"
          << group << ":\n";
      for(const Option* opt: opts) {
         out << make_option(opt, is_positional);
      }

      return out.str();
   }

   std::vector<const Option*> make_groups_ex(std::vector<const Option*> opts, const App* app, AppFormatMode mode, const std::string& group) const {
      std::vector<const Option*> po = app->get_options([app, mode, &group](const Option* opt) {
         return opt->get_group() == group                       // Must be in the right group
                && opt->nonpositional()                         // Must not be a positional
                && (mode != AppFormatMode::Sub                  // If mode is Sub, then
                    || (app->get_help_ptr() != opt              // Ignore help pointer
                        && app->get_help_all_ptr() != opt));    // Ignore help all pointer

      });

      std::vector<const Option*> dest;
      if(!po.empty()) {
         std::set_union(opts.begin(), opts.end(),
                        po.begin(), po.end(),
                        std::back_inserter(dest),
                        [](const Option*& a, const Option*& b) {
                           return a->get_name() != b->get_name();
                        });
      }
      return dest;
   }

   virtual std::string make_groups(const App* app, AppFormatMode mode) const {
      std::stringstream out;
      std::vector<std::string> groups = app->get_groups();

      auto app2 = app;
      const App* parent = app2->get_parent();
      while (parent != nullptr && app2->get_fallthrough())
      {
         auto options = parent->get_options();
         for(const Option * opt : options) {
            // Add group if it is not already in there
            if(std::find(groups.begin(), groups.end(), opt->get_group()) == groups.end()) {
                  groups.push_back(opt->get_group());
            }
         }
         app2 = parent;
         parent = app2->get_parent();
      }


      // Options
      for(const std::string& group: groups) {
         // pull opts for current group
         std::vector<const Option*> opts;
         opts = make_groups_ex(opts, app, mode, group);

         // pull opts for parents with same group type if fallthrough in place
         app2 = app;
         parent = app2->get_parent();
         while(parent != nullptr && app2->get_fallthrough())
         {
            auto opts2 = make_groups_ex(opts, parent, mode, group);
            for (auto && opt : opts2)
            {
               if(std::find(opts.begin(), opts.end(), opt) == opts.end()) {
                  opts.push_back(opt);
               }
            }
            app2 = parent;
            parent = app2->get_parent();
         }

         // convert to stream
         if(!group.empty() && !opts.empty()) {
            out << make_group(group, false, opts);

            if(group != groups.back())
               out << "\n";
         }
      }

      return out.str();
   }
};
// [CLI11:leap_formatter_hpp:end]
}// namespace CLI