/**
 * @file ConsoleView.hpp
 * @brief Ce que le terminal affiche. Aucune decision, que de la mise en forme.
 */

#ifndef CONSOLEVIEW_HPP_
#define CONSOLEVIEW_HPP_

#include "Catalog.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

/**
 * @class ConsoleView
 * @brief Les sorties texte de la borne, et le decoupage des commandes.
 */
class ConsoleView {

    public:
        explicit ConsoleView(Catalog &catalog) : _catalog(catalog) {}

        static std::vector<std::string> split(const std::string &line) {
            std::vector<std::string> words;
            std::istringstream stream(line);
            std::string word;

            while (stream >> word)
                words.push_back(word);
            return words;
        }

        static void say(const std::string &message) {
            std::printf("%s\n", message.c_str());
            std::fflush(stdout);
        }

        void usage() {
            std::string contracts;

            for (const std::string &type : _catalog.GetTypes())
                contracts += (contracts.empty() ? "" : " | ") + type;
            say("list libs | list graphics | list games | list others\n"
                "get graphic | get game\n"
                "set graphic <nom> | set game <nom>\n"
                "unset graphic | unset game\n"
                "unload <nom> | reload\n"
                "quit\n"
                "\nmemes verbes sur un contrat : " + contracts +
                "\n  ex. list audio | set audio raylib | unset audio");
        }

        /**
         * @brief La vue par DLL : une ligne par bibliotheque, puis ses
         *        modules avec leur contrat et leurs detenteurs.
         */
        void showLibraries() {
            const std::vector<Catalog::Entry> libs = _catalog.GetLibraries();

            if (libs.empty())
                return say("(aucune)");
            for (const Catalog::Entry &lib : libs) {
                const unsigned held = _catalog.holders(Catalog::LIBS, lib);

                std::printf("  %c %-14s %s\n", held ? '*' : ' ', lib.key.c_str(),
                            held ? ("tenue x" + std::to_string(held)).c_str() : "libre");
                for (IModule *module : _catalog.modules().GetAllByKey(lib.key))
                    if (module)
                        std::printf("      %-12s (%-9s) %s\n",
                                    module->name(), module->type(),
                                    module->uses()
                                        ? ("tenu x" + std::to_string(module->uses())).c_str()
                                        : "");
            }
            std::fflush(stdout);
        }

        /** @brief Une liste de modules, avec qui les tient. */
        void show(const std::vector<Catalog::Entry> &entries) {
            if (entries.empty())
                return say("(aucun)");
            for (const Catalog::Entry &entry : entries) {
                IModule *module = _catalog.modules().Get(entry.type, entry.key);
                const unsigned held = module ? module->uses() : 0;

                std::printf("  %c %-10s (%-9s) [%s] %s\n", held ? '*' : ' ',
                            entry.name.c_str(), entry.type.c_str(), entry.key.c_str(),
                            held ? ("tenu x" + std::to_string(held)).c_str() : "");
            }
            std::fflush(stdout);
        }

    private:
        Catalog &_catalog;
};

#endif /* !CONSOLEVIEW_HPP_ */
