/**
 * @file Menu.hpp
 * @brief Les colonnes du menu et le curseur qui s'y promene.
 */

#ifndef MENU_HPP_
#define MENU_HPP_

#include "Catalog.hpp"

#include <cctype>
#include <map>
#include <string>
#include <vector>

/**
 * @class Menu
 * @brief Quatre colonnes, un curseur par colonne.
 *
 * Deux d'entre elles ne sont pas des contrats : les bibliotheques chargees,
 * et un fourre-tout ou chaque entree porte son propre type.
 */
class Menu {

    public:
        explicit Menu(Catalog &catalog) : _catalog(catalog) {}

        /** @brief Les colonnes, dans l'ordre d'affichage. */
        std::vector<std::string> columns() const {
            return {Catalog::LIBS, Catalog::GRAPHIC2, Catalog::GAME, Catalog::OTHERS};
        }

        /** @brief La colonne courante. */
        std::string column() const {
            const std::vector<std::string> all = columns();

            return _column < all.size() ? all[_column] : Catalog::GRAPHIC2;
        }

        /** @brief Ce que la colonne courante propose. */
        std::vector<Catalog::Entry> entries() { return entriesOf(column()); }

        /** @brief Ce que cette colonne propose, pseudo-colonnes comprises. */
        std::vector<Catalog::Entry> entriesOf(const std::string &name) {
            if (name == Catalog::LIBS)     return _catalog.GetLibraries();
            if (name == Catalog::GRAPHIC2) return _catalog.GetGraphics();
            if (name == Catalog::OTHERS)   return _catalog.GetOthers();
            return _catalog.of(name);
        }

        /** @brief Le curseur de la colonne courante, cree au besoin. */
        size_t &cursor() { return _cursors[column()]; }

        /** @brief Le curseur de cette colonne, cree au besoin. */
        size_t &cursorOf(const std::string &name) { return _cursors[name]; }

        /** @brief L'indice de la colonne courante. */
        size_t index() const { return _column; }

        /** @brief Passe a la colonne suivante ou precedente, en boucle. */
        void step(int by) {
            const size_t count = columns().size();

            if (count)
                _column = (_column + count + by) % count;
        }

        /** @brief Deplace la selection dans la colonne courante. */
        void move(int by) {
            const size_t count = entries().size();
            size_t &index = cursor();

            if (count == 0)
                return;
            index = (index + count + by) % count;
        }

        /**
         * @brief Recadre chaque curseur sur sa colonne.
         *
         * Une colonne fermee raccourcit les listes : sans ce recadrage le
         * curseur designerait le vide.
         */
        void clampAll() {
            for (const std::string &name : columns())
                clamp(_cursors[name], entriesOf(name).size());
        }

        /** @brief Le titre d'une colonne, en majuscules. */
        static std::string heading(const std::string &name) {
            std::string title = name;

            for (char &c : title)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return title;
        }

        /**
         * @brief Une entree du menu.
         *
         * Deux reperes distincts : le curseur dit ou tu es, l'etoile dit qui
         * est reellement tenu.
         */
        static std::string line(bool selected, unsigned holders, const Catalog::Entry &entry) {
            return std::string(selected ? " > " : "   ") + entry.name +
                   "   (" + entry.type + ")" +
                   (holders ? "  *tenu x" + std::to_string(holders) : "") + "\n";
        }

    private:
        static void clamp(size_t &index, size_t count) {
            if (count && index >= count)
                index = count - 1;
        }

        Catalog &_catalog;
        /* Un curseur par colonne, cree a la demande. */
        std::map<std::string, size_t> _cursors;
        size_t _column = 0;
};

#endif /* !MENU_HPP_ */
