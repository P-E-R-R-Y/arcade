/**
 * @file Selection.hpp
 * @brief Ce que la borne affiche comme choisi, contrat par contrat.
 */

#ifndef SELECTION_HPP_
#define SELECTION_HPP_

#include "IModuleManager.hpp"

#include <iterator>
#include <map>
#include <string>

/**
 * @class Selection
 * @brief Le choix de la borne, et rien de plus.
 *
 * Un jeu ne le suit pas : il lit GetAllByType() et prend ce qu'il veut.
 * Ce qui est ici ne concerne donc que la borne elle-meme.
 */
class Selection {

    public:
        explicit Selection(IModuleManager &modules) : _modules(modules) {}

        /** @brief Le module choisi pour ce contrat, ou nullptr. */
        IModule *current(const std::string &type) const {
            const auto found = _current.find(type);

            return found == _current.end() ? nullptr : found->second;
        }

        /** @brief Declare le choix. nullptr pour n'en avoir aucun. */
        void select(const std::string &type, IModule *module) {
            if (module)
                _current[type] = module;
            else
                _current.erase(type);
        }

        /** @brief Oublie ce qui a ete ferme : la table ne le connait plus. */
        void forget() {
            for (auto it = _current.begin(); it != _current.end(); ) {
                bool alive = false;

                for (const std::string &key : _modules.GetKeys())
                    if (_modules.Get(it->first, key) == it->second)
                        alive = true;
                it = alive ? std::next(it) : _current.erase(it);
            }
        }

    private:
        IModuleManager &_modules;
        std::map<std::string, IModule *> _current;
};

#endif /* !SELECTION_HPP_ */
