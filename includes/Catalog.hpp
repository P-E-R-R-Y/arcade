/**
 * @file Catalog.hpp
 * @brief La table vue par la borne : ce qui est charge, et qui le tient.
 */

#ifndef CATALOG_HPP_
#define CATALOG_HPP_

#include "IAppModule.hpp"
#include "IGraphic2Module.hpp"
#include "IGraphic3Module.hpp"
#include "IModuleManager.hpp"

#include <string>
#include <vector>

/**
 * @class Catalog
 * @brief Toutes les questions que la borne pose a la table, et aucune autre.
 *
 * Ne possede rien, n'affiche rien, n'ouvre aucune fenetre : c'est la seule
 * partie de la borne qui se teste sans vendor.
 */
class Catalog {

    public:
        /** @brief Une case de la table, telle qu'un menu l'affiche. */
        struct Entry {
            std::string key;    ///< la colonne : le nom du fichier charge
            std::string name;   ///< ce que le module dit de lui-meme
            std::string type;   ///< la ligne : la famille qu'il declare
        };

        /* Les deux premieres sont des contrats, les deux dernieres non :
         * une colonne de bibliotheques et une colonne fourre-tout. */
        static constexpr const char *GRAPHIC2 = IGraphic2Module::contract;
        static constexpr const char *GRAPHIC3 = IGraphic3Module::contract;
        static constexpr const char *GAME     = IAppModule::contract;
        static constexpr const char *LIBS     = "librairies";
        static constexpr const char *OTHERS   = "autres";

        explicit Catalog(IModuleManager &modules) : _modules(modules) {}

        /** @brief Le manager, pour qui a besoin d'agir et pas seulement de lire. */
        IModuleManager &modules() { return _modules; }

        /** @brief Une ligne de la table, mise en forme pour l'affichage. */
        std::vector<Entry> of(const std::string &type) {
            std::vector<Entry> found;

            for (const std::string &key : _modules.GetKeys())
                if (IModule *module = _modules.Get(type, key))
                    found.push_back({key, module->name(), module->type()});
            return found;
        }

        /**
         * @brief Les bibliotheques chargees.
         *
         * Ce sont elles qu'on charge et decharge : un module ne se retire
         * pas seul, il part avec la sienne.
         */
        std::vector<Entry> GetLibraries() {
            std::vector<Entry> found;

            for (const std::string &key : _modules.GetKeys())
                found.push_back({key, key, "dll"});
            return found;
        }

        /**
         * @brief Toutes les bibliotheques qui savent dessiner en 2D.
         *
         * Deux lignes lues, une seule sorte rendue : un IGraphic3Module EST
         * un IGraphic2Module, mais son type() ne rend que le contrat le plus
         * precis.
         */
        std::vector<IGraphic2Module *> GetGraphicModules() {
            std::vector<IGraphic2Module *> found;

            for (const char *type : {GRAPHIC2, GRAPHIC3})
                for (IModule *module : _modules.GetAllByType(type))
                    if (module)   // un Span rend les trous a nullptr
                        found.push_back(static_cast<IGraphic2Module *>(module));
            return found;
        }

        /** @brief Tous les jeux chargeables. */
        std::vector<IAppModule *> GetGameModules() {
            std::vector<IAppModule *> found;

            for (IModule *module : _modules.GetAllByType(GAME))
                if (module)
                    found.push_back(static_cast<IAppModule *>(module));
            return found;
        }

        /** @brief Les bibliotheques graphiques, pour l'affichage. */
        std::vector<Entry> GetGraphics() {
            std::vector<Entry> found = of(GRAPHIC2);

            for (const Entry &entry : of(GRAPHIC3))
                found.push_back(entry);
            return found;
        }

        /** @brief Les jeux, pour l'affichage. */
        std::vector<Entry> GetGames() { return of(GAME); }

        /**
         * @brief Tout ce qui n'est ni graphique ni jeu.
         *
         * La borne ne sait pas s'en servir et les garde quand meme : un jeu
         * qui connait le contrat d'une physique ira le chercher.
         */
        std::vector<Entry> GetOthers() {
            std::vector<Entry> found;

            for (const std::string &type : _modules.GetTypes())
                if (type != GRAPHIC2 && type != GRAPHIC3 && type != GAME)
                    for (const Entry &entry : of(type))
                        found.push_back(entry);
            return found;
        }

        /** @brief Les familles presentes, quelles qu'elles soient. */
        std::vector<std::string> GetTypes() { return _modules.GetTypes(); }

        /** @brief Ce contrat existe-t-il ? Sinon la commande est inconnue. */
        bool known(const std::string &type) {
            for (const std::string &found : _modules.GetTypes())
                if (found == type)
                    return true;
            return false;
        }

        /**
         * @brief Combien de detenteurs.
         *
         * uses(), pas le choix affiche par la borne : c'est cette detention
         * qui empeche une bibliotheque de se fermer.
         */
        unsigned holders(const std::string &type, const Entry &entry) {
            unsigned count = 0;

            if (type == LIBS) {
                for (IModule *module : _modules.GetAllByKey(entry.key))
                    if (module)
                        count += module->uses();
                return count;
            }

            IModule *module = _modules.Get(entry.type, entry.key);

            return module ? module->uses() : 0;
        }

        /** @brief La bibliotheque d'ou vient ce module, "" si introuvable. */
        std::string keyOf(IModule *module) {
            for (const std::string &key : _modules.GetKeys())
                if (_modules.Get(module->type(), key) == module)
                    return key;
            return "";
        }

        /**
         * @brief Retrouve un module par son nom, ou par sa cle de chargement.
         *
         * Les deux, parce que l'utilisateur voit le nom dans le menu mais que
         * la cle est ce qui identifie vraiment une colonne.
         */
        template <typename T>
        T *find(const std::vector<T *> &modules, const std::string &wanted) {
            for (T *module : modules)
                if (wanted == module->name())
                    return module;
            for (const std::string &key : _modules.GetKeys())
                if (key == wanted)
                    for (T *module : modules)
                        if (module == _modules.Get(module->type(), key))
                            return module;
            return nullptr;
        }

    private:
        IModuleManager &_modules;
};

#endif /* !CATALOG_HPP_ */
