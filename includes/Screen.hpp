/**
 * @file Screen.hpp
 * @brief La fenetre de la borne, et le vendor qui l'a fabriquee.
 */

#ifndef SCREEN_HPP_
#define SCREEN_HPP_

#include "Catalog.hpp"
#include "Games.hpp"
#include "Menu.hpp"
#include "Selection.hpp"

#include <cstdio>
#include <string>
#include <vector>

/**
 * @class Screen
 * @brief Tient le vendor graphique de la borne, sa fenetre, et dessine le menu.
 *
 * La borne peut vivre sans : sans fenetre elle attend, pilotable au
 * terminal.
 */
class Screen {

    public:
        static constexpr int32_t WIDTH = 900;
        static constexpr int32_t HEIGHT = 560;

        Screen(Catalog &catalog, Menu &menu, Games &games, Selection &selection,
               const std::string &assets, std::string &status)
            : _catalog(catalog), _menu(menu), _games(games), _selection(selection),
              _assets(assets), _status(status) {}

        ~Screen() { release(); }

        Screen(const Screen &) = delete;
        Screen &operator=(const Screen &) = delete;

        graphic::IWindow2 *window() const { return _window; }
        graphic::IKeyboard *keyboard() const { return _keyboard; }
        IGraphic2Module *using_() const { return _using; }

        /** @brief Le joueur a-t-il ferme la fenetre ? */
        bool closing() const { return _window && !_window->isOpen(); }

        /**
         * @brief Bascule sur ce module.
         *
         * Prend le MODULE, pas un indice : la liste change des qu'on charge
         * ou decharge.
         */
        void use(IGraphic2Module *module) {
            if (!module || module == _using)
                return;

            /* QUATRE TEMPS, et l'ordre est tout. L'ancien doit etre
             * ENTIEREMENT eteint avant que le nouveau ne s'allume : ils se
             * partagent des globales de processus - un contexte OpenGL entre
             * raylib et sfml, un unique libSDL3 sous sdl2-compat.
             *
             * 1. plus personne en service : les invites lisent nullptr et
             *    lachent SANS se rebrancher, donc sans rafler la fenetre.
             * 2. les invites lachent, pendant que l'ancien vit encore.
             * 3. l'hote rend tout.
             * 4. seulement maintenant, le nouveau s'allume. */
            _selection.select(Catalog::GRAPHIC2, nullptr);
            _games.update();
            release();

            _selection.select(Catalog::GRAPHIC2, module);
            module->acquire();
            _using = module;
            _window = module->createWindow(WIDTH, HEIGHT, "P-E-R-R-Y arcade");

            /* Un invite a pu prendre l'unique fenetre du vendor a l'etape 2,
             * juste avant nous. L'hote passe avant : sans fenetre il ne peut
             * plus meme recevoir echap, alors qu'un jeu sait continuer
             * aveugle. */
            if (!_window && !_games.empty()) {
                _status = std::string(_games.first()->name()) + " arrete : " +
                          module->name() + " ne donne qu'une fenetre";
                _games.quit();
                _window = module->createWindow(WIDTH, HEIGHT, "P-E-R-R-Y arcade");
            }

            if (!_window) {
                /* On reste, aveugle : "set graphic <nom>" la ramene. */
                _status = std::string(module->name()) + " ne donne pas de fenetre";
                std::printf("%s ne donne pas de fenetre\n", module->name());
                std::fflush(stdout);
                return;
            }

            _window->setFrameLimit(60);
            _keyboard = module->createKeyboard(_window);
            _font = module->createFont(_assets + "/font.ttf");
            _title = module->createText("", _font);
            _body = module->createText("", _font);
            _hint = module->createText("", _font);
        }

        /**
         * @brief Rend au vendor courant tout ce qu'il avait fabrique.
         *
         * LE MODULE SEUL DECIDE, pas la fenetre : un vendor qui a refuse
         * d'en ouvrir une avait quand meme ete acquis.
         */
        void release() {
            if (!_using)
                return;

            IGraphic2Module *module = _using;

            /* Chacun teste : sur le chemin du refus il n'y a rien a rendre. */
            if (_hint)     module->deleteText(_hint);
            if (_body)     module->deleteText(_body);
            if (_title)    module->deleteText(_title);
            if (_font)     module->deleteFont(_font);
            if (_keyboard) module->deleteKeyboard(_keyboard);
            if (_window)   module->deleteWindow(_window);

            /* Le relachement APRES les destructions, jamais avant : entre
             * les deux, sa dll pourrait se fermer. */
            module->release();

            _hint = nullptr;
            _body = nullptr;
            _title = nullptr;
            _font = nullptr;
            _keyboard = nullptr;
            _window = nullptr;
            _using = nullptr;
        }

        /**
         * @brief Si ma bibliotheque est condamnee : lacher, puis basculer.
         *
         * Sans remplacant, la borne RESTE, aveugle.
         */
        void evacuate() {
            if (!_using || !_using->mustClose())
                return;

            release();

            for (IGraphic2Module *module : _catalog.GetGraphicModules())
                if (!module->mustClose())
                    return use(module);

            _status = "plus aucune bibliotheque : reload, puis set graphic <nom>";
            std::printf("%s\n", _status.c_str());
            std::fflush(stdout);
        }

        /**
         * @brief Le menu : une colonne par famille, cote a cote.
         *
         * Les fleches gauche et droite passent de l'une a l'autre.
         */
        void draw() {
            if (!_window)
                return;

            _title->setFont(_font);
            _title->setFontSize(28);
            _title->setTextColor({255, 205, 80, 255});
            _title->setPosition({40.f, 30.f});
            _title->setText("P-E-R-R-Y  arcade");
            _window->drawText(_title);

            const std::vector<std::string> all = _menu.columns();
            const float width = (WIDTH - 2 * MARGIN) / (all.empty() ? 1 : all.size());

            for (size_t c = 0; c < all.size(); c++) {
                const bool here = (c == _menu.index());
                const std::vector<Catalog::Entry> list = _menu.entriesOf(all[c]);
                std::string text = (here ? "> " : "  ") + Menu::heading(all[c]) + "\n\n";

                if (list.empty())
                    text += "   (aucun)\n";
                for (size_t i = 0; i < list.size(); i++)
                    text += Menu::line(here && i == _menu.cursorOf(all[c]),
                                       _catalog.holders(all[c], list[i]), list[i]);

                _body->setFont(_font);
                _body->setFontSize(18);
                _body->setTextColor(here ? Color{235, 235, 240, 255}
                                         : Color{130, 135, 150, 255});
                _body->setPosition({MARGIN + c * width, 100.f});
                _body->setText(text);
                _window->drawText(_body);
            }

            _hint->setFont(_font);
            _hint->setFontSize(16);
            _hint->setTextColor({150, 160, 190, 255});
            _hint->setPosition({40.f, HEIGHT - 60.f});
            _hint->setText("fleches   entree : appliquer   retour : decharger   R : relire lib/   echap"
                           + (_status.empty() ? std::string() : "        " + _status));
            _window->drawText(_hint);
        }

    private:
        static constexpr float MARGIN = 40.f;

        Catalog &_catalog;
        Menu &_menu;
        Games &_games;
        Selection &_selection;
        std::string _assets;
        std::string &_status;

        IGraphic2Module *_using = nullptr;

        /* Fabriques par le vendor courant, donc detruits par lui avant tout
         * changement. Ils ne survivent pas a un release(). */
        graphic::IWindow2 *_window = nullptr;
        graphic::IKeyboard *_keyboard = nullptr;
        graphic::IFont *_font = nullptr;
        graphic::IText *_title = nullptr;
        graphic::IText *_body = nullptr;
        graphic::IText *_hint = nullptr;
};

#endif /* !SCREEN_HPP_ */
