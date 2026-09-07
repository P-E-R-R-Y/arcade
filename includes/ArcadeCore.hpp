/**
 * @file ArcadeCore.hpp
 * @brief La borne : elle charge des dll, les range par type, et laisse choisir.
 *
 * Elle ne connait aucun vendor et aucun jeu. Elle ne connait meme pas les
 * contrats qu'ils remplissent : un IModuleManager charge tout ce qui exporte
 * getModules() et trie par IModule::type(). Elle n'est consommatrice que de
 * graphic2, dont elle tire sa fenetre, et de game, qu'elle lance.
 *
 * Ce fichier ne fait que cabler : le catalogue, le menu, l'ecran, les jeux
 * et le terminal vivent chacun chez eux.
 */

#ifndef ARCADECORE_HPP_
#define ARCADECORE_HPP_

#include "Catalog.hpp"
#include "Console.hpp"
#include "ConsoleView.hpp"
#include "Games.hpp"
#include "ICore.hpp"
#include "Menu.hpp"
#include "Screen.hpp"
#include "Selection.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

/**
 * @class ArcadeCore
 * @brief Une IApp : le lanceur a sa propre boucle event / update / display.
 */
class ArcadeCore : public IApp {

    public:
        /**
         * @brief Ouvre toutes les bibliotheques du dossier, puis la fenetre.
         *
         * @param libraries dossier balaye au demarrage
         * @param assets    dossier des polices et images de la borne
         */
        ArcadeCore(const std::string &libraries, const std::string &assets)
            : _libraries(libraries), _catalog(_modules), _menu(_catalog),
              _games(_modules), _selection(_modules),
              _screen(_catalog, _menu, _games, _selection, assets, _status),
              _view(_catalog) {
            scan();

            const std::vector<IGraphic2Module *> graphics = _catalog.GetGraphicModules();

            if (!graphics.empty())
                _screen.use(graphics.front());
        }

        /** @brief Les jeux d'abord, l'ecran ensuite : ils dessinent dedans. */
        ~ArcadeCore() override { _games.quit(); _screen.release(); }

        /** @brief Le manager, pour qui saura en faire quelque chose. */
        IModuleManager &GetRegistry() { return _modules; }

    protected:
        void event() override {
            using KB = graphic::IKeyboard;

            /* Le terminal D'ABORD, et toujours : c'est la seule entree qui
             * ne depend d'aucun vendor. Sans elle, une borne sans fenetre
             * n'aurait aucun moyen d'en retrouver une. */
            while (const std::optional<std::string> line = _console.poll())
                command(*line);

            /* Sans fenetre la borne ATTEND, elle ne meurt pas. */
            if (!_screen.window() || !_screen.keyboard())
                return _games.event();

            /* Les fronts, dans la condition : sans evenement ils sont faux
             * de toute facon. */
            if (_screen.window()->pollEvent()) {
                _screen.window()->eventClose();

                graphic::IKeyboard *keys = _screen.keyboard();

                if (keys->isKeyPressed(KB::KEY_ESCAPE))    return stop();
                if (keys->isKeyPressed(KB::KEY_LEFT))      _menu.step(-1);
                if (keys->isKeyPressed(KB::KEY_RIGHT))     _menu.step(+1);
                if (keys->isKeyPressed(KB::KEY_UP))        _menu.move(-1);
                if (keys->isKeyPressed(KB::KEY_DOWN))      _menu.move(+1);

                /* Ces trois-la refont le monde : activate() peut basculer de
                 * vendor, et l'ecran detruit alors sa fenetre et son clavier
                 * sous nos pieds. On rend la main plutot que de relire le
                 * clavier de l'ancien. */
                if (keys->isKeyPressed(KB::KEY_ENTER))     return activate();
                if (keys->isKeyPressed(KB::KEY_BACKSPACE)) return unload();
                if (keys->isKeyPressed(KB::KEY_R))         return reload();
            }

            /* TOUJOURS, meme si notre fenetre n'a rien recu : le jeu a
             * peut-etre la sienne, et une fenetre qu'on ne pompe pas devient
             * inerte. Il recoit l'etape event(), jamais run(). */
            _games.event();
        }

        void update() override {
            if (_screen.closing())
                return stop();

            /* Ma bibliotheque a-t-elle ete condamnee ? Si oui je lache et je
             * bascule, sinon Reconcile() ne fermerait jamais rien. */
            _screen.evacuate();

            const std::string stopped = _games.dropCondemned();

            if (!stopped.empty())
                _status = stopped + " decharge";

            /* A chaque tick, sans exception. Ce qui est condamne et libre se
             * ferme ; le reste repasse au tour suivant. */
            if (_modules.Reconcile())
                _selection.forget();

            _menu.clampAll();
            _games.update();
            _games.dropStopped();
        }

        void display() override {
            /* PLANCHER : endDraw() est le seul frein de la boucle, et il n'a
             * lieu que s'il y a une fenetre. Sans vendor, run() brulerait un
             * coeur pour ne rien afficher. */
            if (!_screen.window())
                return std::this_thread::sleep_for(std::chrono::milliseconds(16));

            _screen.window()->beginDraw();
            _screen.draw();

            /* Le jeu dessine ICI, entre le begin et le end de la borne : la
             * frame appartient a qui possede la fenetre. */
            _games.display();
            _screen.window()->endDraw();
        }

    private:
        /* ---- ce que le menu declenche -------------------------------- */

        /** @brief Entree : basculer de vendor, lancer un jeu, ou choisir. */
        void activate() {
            const std::string type = _menu.column();
            const std::vector<Catalog::Entry> list = _menu.entries();

            if (_menu.cursor() >= list.size())
                return;

            if (type == Catalog::GRAPHIC2) {
                const std::vector<IGraphic2Module *> graphics = _catalog.GetGraphicModules();

                if (_menu.cursor() < graphics.size())
                    _screen.use(graphics[_menu.cursor()]);
                return;
            }

            if (type == Catalog::GAME) {
                const std::vector<IAppModule *> games = _catalog.GetGameModules();

                if (_menu.cursor() >= games.size())
                    return;
                if (_games.running(games[_menu.cursor()]))
                    return _games.quit(games[_menu.cursor()]);   // entree l'arrete
                _status = _games.play(games[_menu.cursor()]);
                return;
            }

            /* Une bibliotheque ne se met pas "en service" : elle se charge
             * ou se decharge. Retour arriere s'en occupe. */
            if (type == Catalog::LIBS) {
                _status = list[_menu.cursor()].key + " : retour pour la decharger";
                return;
            }

            /* Colonne fusionnee : on arbitre le contrat de l'ENTREE. */
            const std::string contract = (type == Catalog::OTHERS)
                ? list[_menu.cursor()].type : type;
            IModule *picked = _modules.Get(contract, list[_menu.cursor()].key);

            _selection.select(contract,
                              _selection.current(contract) == picked ? nullptr : picked);
            _status = contract + " : " +
                      (_selection.current(contract) ? list[_menu.cursor()].name : "aucun");
        }

        /**
         * @brief Condamne la bibliotheque selectionnee.
         *
         * Rien ne ferme ici : Unload() pose le drapeau, evacuate() lache, et
         * Reconcile() ferme - deux etapes plus loin, au meme tick.
         */
        void unload() {
            const std::vector<Catalog::Entry> list = _menu.entries();

            if (_menu.cursor() >= list.size())
                return;
            _modules.Unload(list[_menu.cursor()].key);
        }

        /**
         * @brief Relit le dossier : ce qui est apparu depuis est charge.
         *
         * Load() refuse deja une cle presente, donc un second passage ne
         * recharge pas ce qui est deja la.
         */
        void reload() {
            const size_t before = _modules.GetKeys().size();
            const size_t seen = scan();
            const size_t added = _modules.GetKeys().size() - before;

            _status = std::to_string(added) + " ajoutee(s) sur " +
                      std::to_string(seen) + " fichier(s) lu(s)";
        }

        /**
         * @brief Charge chaque bibliotheque du dossier, sous une cle = son nom.
         *
         * Une bibliotheque qui ne s'ouvre pas est ignoree, comme celle qui
         * n'exporte pas getModules() : un dossier de greffons finit toujours
         * par contenir un intrus.
         */
        size_t scan() {
            namespace fs = std::filesystem;
            size_t seen = 0;

            if (!fs::is_directory(_libraries))
                return 0;

            for (const fs::directory_entry &file : fs::directory_iterator(_libraries)) {
                if (file.path().extension() != SharedLibrary::extension())
                    continue;
                seen++;

                try {
                    _modules.Load(file.path().string(), file.path().stem().string());
                } catch (const std::exception &) {
                    continue;   // pas une bibliotheque chargeable, on passe
                }
            }
            return seen;
        }

        /* ---- le terminal --------------------------------------------- */

        void setGraphic(const std::string &name) {
            IGraphic2Module *module = _catalog.find(_catalog.GetGraphicModules(), name);

            if (!module)
                return ConsoleView::say("bibliotheque inconnue : " + name);
            _screen.use(module);
            ConsoleView::say(_screen.window() ? "en service : " + name
                                              : name + " ne donne pas de fenetre");
        }

        void setGame(const std::string &name) {
            IAppModule *module = _catalog.find(_catalog.GetGameModules(), name);

            if (!module)
                return ConsoleView::say("jeu inconnu : " + name);

            const std::string failed = _games.play(module);

            ConsoleView::say(failed.empty() ? "lance : " + name : failed);
        }

        /** @brief Met un module en service sur un contrat quelconque. */
        void setModule(const std::string &type, const std::string &name) {
            for (const Catalog::Entry &entry : _catalog.of(type))
                if (entry.name == name || entry.key == name) {
                    _selection.select(type, _modules.Get(type, entry.key));
                    return ConsoleView::say(type + " en service : " + entry.name);
                }
            ConsoleView::say("aucun " + type + " sous ce nom : " + name);
        }

        void doUnload(const std::string &name) {
            for (const std::string &key : _modules.GetKeys())
                if (key == name) {
                    _modules.Unload(key);
                    return ConsoleView::say(name + " condamne, fermeture des qu'il sera libre");
                }
            for (const Catalog::Entry &entry : _catalog.of(Catalog::GRAPHIC2))
                if (entry.name == name) {
                    _modules.Unload(entry.key);
                    return ConsoleView::say(name + " condamne, fermeture des qu'il sera libre");
                }
            ConsoleView::say("rien a decharger sous ce nom : " + name);
        }

        /**
         * @brief Une commande tapee au terminal.
         *
         * Meme vocabulaire que le menu, memes fonctions appelees : ce n'est
         * pas un second chemin, c'est une seconde entree sur le meme.
         */
        void command(const std::string &line) {
            const std::vector<std::string> words = ConsoleView::split(line);

            if (words.empty())
                return;

            const std::string &verb = words[0];
            const std::string what = words.size() > 1 ? words[1] : "";
            const std::string name = words.size() > 2 ? words[2] : "";

            if (verb == "help")   return _view.usage();
            if (verb == "quit" || verb == "exit") return stop();
            if (verb == "reload") { reload(); return ConsoleView::say(_status); }

            if (verb == "list" && what == "graphics") return _view.show(_catalog.GetGraphics());
            if (verb == "list" && what == "games")    return _view.show(_catalog.GetGames());
            if (verb == "list" && what == "others")   return _view.show(_catalog.GetOthers());
            if (verb == "list" && (what == "libs" || what == "libraries"))
                return _view.showLibraries();

            if (verb == "get" && what == "graphic")
                return ConsoleView::say(_screen.using_() ? _screen.using_()->name() : "(aucune)");
            if (verb == "get" && what == "game")
                return ConsoleView::say(_games.empty() ? "(aucun)" : _games.names());

            if (verb == "set" && what == "graphic")   return setGraphic(name);
            if (verb == "set" && what == "game")      return setGame(name);

            if (verb == "unset" && what == "graphic") {
                _screen.release();
                return ConsoleView::say("aucune bibliotheque en service");
            }
            if (verb == "unset" && what == "game") {
                _games.quit();
                return ConsoleView::say("aucun jeu");
            }

            if (verb == "unload") return doUnload(what);

            /* Tout autre contrat decouvert au chargement - "audio" en est un.
             * Les memes verbes, sans une ligne par famille. */
            if (_catalog.known(what)) {
                if (verb == "list") return _view.show(_catalog.of(what));
                if (verb == "get") {
                    IModule *chosen = _selection.current(what);

                    return ConsoleView::say(chosen ? chosen->name() : "(aucun)");
                }
                if (verb == "set")   return setModule(what, name);
                if (verb == "unset") {
                    _selection.select(what, nullptr);
                    return ConsoleView::say("aucun " + what + " en service");
                }
            }

            ConsoleView::say("commande inconnue : " + verb + "   (help)");
        }

        /* L'ordre de declaration EST l'ordre de construction : _modules
         * d'abord, puis tout ce qui le reference. */
        IModuleManager _modules;
        std::string _libraries;
        std::string _status;   ///< resultat de la derniere action, affiche en bas

        Catalog _catalog;
        Menu _menu;
        Games _games;
        Selection _selection;
        Screen _screen;
        ConsoleView _view;
        Console _console;
};

#endif /* !ARCADECORE_HPP_ */
