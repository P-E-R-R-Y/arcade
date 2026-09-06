/**
 * @file ArcadeCore.hpp
 * @brief La borne : elle charge des dll, les range par type, et laisse choisir.
 *
 * Elle ne connait aucun vendor et aucun jeu. Elle ne connait meme pas les
 * contrats qu'ils remplissent : un IModuleManager charge tout ce qui exporte
 * getModules() et trie par IModule::type() - une table decouverte au
 * chargement, pas des colonnes nommees a la compilation. Une borne doit
 * pouvoir lister une physique ou un reseau qu'elle ne sait pas utiliser,
 * pour qu'un jeu qui connait ce contrat puisse le reclamer.
 */

#ifndef ARCADECORE_HPP_
#define ARCADECORE_HPP_

#include "Console.hpp"
#include "IAppModule.hpp"
#include "ICore.hpp"
#include "IGraphic2Module.hpp"
#include "IGraphic3Module.hpp"
#include "IModuleManager.hpp"

#include <cctype>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <map>
#include <cstdio>
#include <sstream>
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
         * @brief Les familles que la borne sait presenter elle-meme.
         *
         * Les chaines viennent des contrats, jamais ecrites en dur ici : une
         * faute de frappe viderait une ligne sans que rien ne le signale.
         */
        static constexpr const char *GRAPHIC2 = IGraphic2Module::contract;
        static constexpr const char *GRAPHIC3 = IGraphic3Module::contract;
        static constexpr const char *GAME     = IAppModule::contract;

        /* Deux colonnes qui ne sont pas des contrats : les bibliotheques
         * chargees, et tous les autres contrats regroupes. */
        static constexpr const char *LIBS   = "librairies";
        static constexpr const char *OTHERS = "autres";

        /**
         * @brief Ouvre toutes les bibliotheques du dossier, puis la fenetre.
         *
         * @param libraries dossier balaye au demarrage
         * @param assets    dossier des polices et images de la borne
         */
        ArcadeCore(const std::string &libraries, const std::string &assets)
            : _libraries(libraries), _assets(assets) {
            scan();

            const std::vector<IGraphic2Module *> graphics = GetGraphicModules();

            if (!graphics.empty())
                use(graphics.front());




        }

        ~ArcadeCore() override { quit(); release(); }

        /** @brief Une case de la table, telle qu'un menu l'affiche. */
        struct Entry {
            std::string key;    ///< la colonne : le nom du fichier charge
            std::string name;   ///< ce que le module dit de lui-meme
            std::string type;   ///< la ligne : la famille qu'il declare
        };

        /**
         * @brief Toutes les bibliotheques qui savent dessiner en 2D.
         *
         * DEUX lignes lues, une seule sorte rendue : un IGraphic3Module EST
         * un IGraphic2Module, mais son type() ne rend que le contrat le plus
         * precis. La table ignore la chaine d'heritage - c'est la borne qui
         * la connait, parce qu'elle a compile les deux en-tetes.
         *
         * Le static_cast est sur pour la meme raison. Un dynamic_cast
         * repondrait faux : sous RTLD_LOCAL le typeinfo n'est pas partage
         * entre la dll et l'hote.
         *
         * @return std::vector<IGraphic2Module *>
         */
        std::vector<IGraphic2Module *> GetGraphicModules() {
            std::vector<IGraphic2Module *> found;

            for (const char *type : {GRAPHIC2, GRAPHIC3})
                for (IModule *module : _modules.GetAllByType(type))
                    if (module)   // un Span rend les trous a nullptr
                        found.push_back(static_cast<IGraphic2Module *>(module));
            return found;
        }

        /**
         * @brief Les bibliotheques chargees, une par colonne de la table.
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

        /** @brief Tous les jeux chargeables. */
        std::vector<IAppModule *> GetGameModules() {
            std::vector<IAppModule *> found;

            for (IModule *module : _modules.GetAllByType(GAME))
                if (module)   // un Span rend les trous a nullptr
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
         * La borne ne sait pas s'en servir et les garde quand meme : elle
         * n'est pas seule a lire ce dossier. Un jeu qui connait le contrat
         * d'une physique ira le chercher par son type().
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

            /* Plus de stop(84) ici : sans fenetre la borne ATTEND, elle ne
             * meurt pas. Le terminal permet d'en redemander une. */
            if (!_window || !_keyboard) {
                if (_app)
                    _app->event();
                return;
            }

            /* Les fronts, dans la condition : sans evenement ils sont faux
             * de toute facon. isKeyDown resterait vrai et repeterait. */
            if (_window->pollEvent()) {
                _window->eventClose();

                if (_keyboard->isKeyPressed(KB::KEY_ESCAPE))    return stop();
                if (_keyboard->isKeyPressed(KB::KEY_LEFT))      step(-1);
                if (_keyboard->isKeyPressed(KB::KEY_RIGHT))     step(+1);
                if (_keyboard->isKeyPressed(KB::KEY_UP))        move(-1);
                if (_keyboard->isKeyPressed(KB::KEY_DOWN))      move(+1);
                /* Ces trois-la refont le monde : activate() peut basculer
                 * de vendor, et release() detruit alors _window et _keyboard
                 * sous nos pieds. On rend la main plutot que de relire le
                 * clavier de l'ancien. C'est la discipline que follow()
                 * impose aux invites, et que la borne ne s'appliquait pas. */
                if (_keyboard->isKeyPressed(KB::KEY_ENTER))     return activate();
                if (_keyboard->isKeyPressed(KB::KEY_BACKSPACE)) return unload();
                if (_keyboard->isKeyPressed(KB::KEY_R))         return reload();
            }

            /* TOUJOURS, meme si notre fenetre n'a rien recu : le jeu a
             * peut-etre la sienne, et une fenetre qu'on ne pompe pas devient
             * inerte au bout de quelques secondes.
             *
             * Il recoit l'etape event(), jamais run() - qui ne rendrait pas
             * la main. C'est pour ca qu'ITickable est public. */
            if (_app)
                _app->event();
        }

        void update() override {
            if (_window && !_window->isOpen())
                return stop();

            /* Ma bibliotheque a-t-elle ete condamnee ? Si oui je lache et je
             * bascule, sinon Reconcile() ne fermerait jamais rien : c'est moi
             * qui tiens le compteur a un. */
            evacuate();

            /* Meme raison pour le jeu : c'est l'arcade qui le tient, donc
             * c'est a elle de lacher quand il est condamne. Un jeu ne se
             * remplace pas - on s'arrete la et le menu revient. */
            if (_running && _running->mustClose()) {
                _status = std::string(_running->name()) + " decharge";
                quit();
            }

            /* A chaque tick, sans exception. Ce qui est condamne et libre se
             * ferme ; le reste repasse au tour suivant. */
            if (_modules.Reconcile())
                forget();   // une colonne fermee peut etre dans _current

            /* Une colonne fermee raccourcit les listes sous le curseur. Sans
             * ce recadrage il designerait le vide, et plus rien ne serait
             * surligne jusqu'a la prochaine fleche. */
            for (const std::string &type : columns())
                clamp(_cursors[type], (type == GRAPHIC2 ? GetGraphics() : of(type)).size());




            if (_app)
                _app->update();

            /* Un jeu qui s'arrete lui-meme rend la main a la borne, il ne la
             * ferme pas. running() est vrai des la construction, donc ce test
             * ne se declenche que sur un stop() volontaire. */
            if (_app && !_app->running())
                quit();
        }

        void display() override {
            /* PLANCHER. endDraw() est le seul frein de la boucle, et il n'a
             * lieu que s'il y a une fenetre. Sans vendor, run() tourne a
             * plusieurs millions de tours par seconde et brule un coeur pour
             * ne rien afficher.
             *
             * Ici et pas dans run() : la temporisation est une politique
             * d'hote, et run() est final dans icore - il sert aussi des
             * applications qui se cadencent elles-memes. */
            if (!_window)
                return std::this_thread::sleep_for(std::chrono::milliseconds(16));

            _window->beginDraw();
            draw();

            /* Le jeu dessine ICI, entre le begin et le end de la borne. Il
             * n'ouvre pas la frame : elle appartient a qui possede la
             * fenetre, et ce n'est pas lui. */
            if (_app)
                _app->display();
            _window->endDraw();
        }

    private:
        /**
         * @brief Les contrats presentes, dans l'ordre : graphique, jeux,
         *        puis tout ce que le dossier a apporte d'autre.
         *
         * L'arcade ARBITRE tous les contrats, elle n'en COMPREND qu'un. Pour
         * declarer qui est en service il suffit d'une chaine - un module
         * reseau se selectionne exactement comme un vendor graphique, sans
         * que la borne sache ce qu'est un reseau. Elle n'est consommatrice
         * que de graphic2, parce qu'elle en tire sa fenetre.
         *
         * GetTypes() rend les lignes decouvertes au chargement : une famille
         * de modules qui n'existait pas hier apparait sans recompiler.
         */
        std::vector<std::string> columns() {
            return {LIBS, GRAPHIC2, IAppModule::contract, OTHERS};
        }

        /** @brief Le contrat de la colonne courante. */
        std::string column() {
            const std::vector<std::string> all = columns();

            return _column < all.size() ? all[_column] : GRAPHIC2;
        }

        /** @brief Ce que la colonne courante propose. */
        std::vector<Entry> entries() { return entriesOf(column()); }

        /** @brief Ce que cette colonne propose, pseudo-colonnes comprises. */
        std::vector<Entry> entriesOf(const std::string &name) {
            if (name == LIBS)     return GetLibraries();
            if (name == GRAPHIC2) return GetGraphics();
            if (name == OTHERS)   return GetOthers();
            return of(name);
        }

        /** @brief Le curseur de la colonne courante, cree au besoin. */
        size_t &cursor() { return _cursors[column()]; }


        static void clamp(size_t &index, size_t count) {
            if (count && index >= count)
                index = count - 1;
        }

        /* ---- les aides du terminal ---------------------------------- */

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

            for (const std::string &type : _modules.GetTypes())
                contracts += (contracts.empty() ? "" : " | ") + type;
            say("list graphics | list games\n"
                "get graphic | get game\n"
                "set graphic <nom> | set game <nom>\n"
                "unset graphic | unset game\n"
                "unload <nom> | reload\n"
                "quit\n"
                "\nmemes verbes sur un contrat : " + contracts +
                "\n  ex. list audio | set audio raylib | unset audio");
        }

        /**
         * @brief Les bibliotheques chargees, et ce que chacune apporte.
         *
         * C'est la vue par DLL : une ligne par bibliotheque, puis ses
         * modules avec leur contrat.
         */
        void showLibraries() {
            const std::vector<Entry> libs = GetLibraries();

            if (libs.empty())
                return say("(aucune)");
            for (const Entry &lib : libs) {
                std::printf("  %c %s\n",
                            (_using && keyOf(_using) == lib.key) ? '*' : ' ',
                            lib.key.c_str());
                for (IModule *module : _modules.GetAllByKey(lib.key))
                    if (module)
                        std::printf("      %-12s (%s)\n",
                                    module->name(), module->type());
            }
            std::fflush(stdout);
        }

        /** @brief Une colonne du menu, avec ce qui est en service marque. */
        void show(const std::vector<Entry> &entries, const std::string &type = "") {
            IModule *inService = type.empty() ? nullptr : current(type);

            if (entries.empty())
                return say("(aucun)");
            for (const Entry &entry : entries) {
                /* Sur un contrat nomme, SEUL ce contrat compte : sdl2 peut
                 * tenir la fenetre sans tenir le son, et les deux modules
                 * portent le meme nom. Les marquer ensemble ferait passer
                 * sdl2 pour le vendor audio en service. */
                const bool live = type.empty()
                    ? ((_using && entry.name == _using->name())
                       || (_running && entry.name == _running->name()))
                    : (inService && entry.name == inService->name());

                std::printf("  %c %-10s [%s]\n", live ? '*' : ' ',
                            entry.name.c_str(), entry.key.c_str());
            }
            std::fflush(stdout);
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

        void setGraphic(const std::string &name) {
            IGraphic2Module *module = find(GetGraphicModules(), name);

            if (!module)
                return say("bibliotheque inconnue : " + name);
            use(module);
            say(_window ? "en service : " + name : name + " ne donne pas de fenetre");
        }

        void setGame(const std::string &name) {
            IAppModule *module = find(GetGameModules(), name);

            if (!module)
                return say("jeu inconnu : " + name);
            play(module);
            say("lance : " + name);
        }

        /** @brief Ce contrat existe-t-il ? Sinon la commande est inconnue. */
        bool known(const std::string &type) {
            for (const std::string &found : _modules.GetTypes())
                if (found == type)
                    return true;
            return false;
        }

        /**
         * @brief Met un module en service sur un contrat quelconque.
         *
         * Le pendant en ligne de commande de la branche generique
         * d'activate() : declarer qui sert ne demande qu'une chaine, donc
         * la borne arbitre un contrat qu'elle ne comprend pas. C'est ce qui
         * permet de changer de vendor audio SANS ECRAN - le seul moment ou
         * l'on en a vraiment besoin.
         */
        void setModule(const std::string &type, const std::string &name) {
            for (const Entry &entry : of(type))
                if (entry.name == name || entry.key == name) {
                    select(type, _modules.Get(type, entry.key));
                    return say(type + " en service : " + entry.name);
                }
            say("aucun " + type + " sous ce nom : " + name);
        }

        void doUnload(const std::string &name) {
            for (const std::string &key : _modules.GetKeys())
                if (key == name) {
                    _modules.Unload(key);
                    return say(name + " condamne, fermeture des qu'il sera libre");
                }
            for (const Entry &entry : of(GRAPHIC2))
                if (entry.name == name) {
                    _modules.Unload(entry.key);
                    return say(name + " condamne, fermeture des qu'il sera libre");
                }
            say("rien a decharger sous ce nom : " + name);
        }

        /** @brief Passe a la colonne suivante ou precedente, en boucle. */
        void step(int by) {
            const size_t count = columns().size();

            if (count)
                _column = (_column + count + by) % count;
        }

        /** @brief Deplace la selection dans la colonne courante. */
        void move(int step) {
            const size_t count = entries().size();
            size_t &index = cursor();

            if (count == 0)
                return;
            index = (index + count + step) % count;
        }

        /**
         * @brief Entree : changer de vendor, ou lancer un jeu.
         *
         * Changer de vendor detruit tout ce que l'ancien avait fabrique
         * AVANT d'en fabriquer de nouveau. raylib n'ouvre qu'une fenetre par
         * processus : sans cet ordre, son deuxieme createWindow() rendrait
         * nullptr et la borne resterait aveugle.
         */
        void activate() {
            const std::string type = column();
            const std::vector<Entry> list = entries();

            if (cursor() >= list.size())
                return;

            if (type == GRAPHIC2) {
                const std::vector<IGraphic2Module *> graphics = GetGraphicModules();

                if (cursor() < graphics.size())
                    use(graphics[cursor()]);
                return;
            }

            if (type == IAppModule::contract) {
                const std::vector<IAppModule *> games = GetGameModules();

                if (cursor() >= games.size())
                    return;
                if (_running == games[cursor()])
                    return quit();   // deja lance : entree l'arrete
                return play(games[cursor()]);
            }

            /* Un contrat que la borne ne comprend pas : elle l'arbitre quand
             * meme. Declarer qui est en service ne demande qu'une chaine, et
             * c'est tout ce dont un invite a besoin pour le trouver. */
            /* Une bibliotheque ne se met pas "en service" : elle se charge
             * ou se decharge. Retour arriere s'en occupe. */
            if (type == LIBS) {
                _status = list[cursor()].key + " : retour pour la decharger";
                return;
            }

            /* Colonne fusionnee : on arbitre le contrat de l'ENTREE. */
            const std::string contract =
                (type == OTHERS) ? list[cursor()].type : type;
            IModule *picked = _modules.Get(contract, list[cursor()].key);

            select(contract, current(contract) == picked ? nullptr : picked);
            _status = contract + " : " +
                      (current(contract) ? list[cursor()].name : "aucun");
        }

        /**
         * @brief Lance un jeu. Il se debrouille pour trouver de quoi dessiner.
         *
         * On ne lui passe ni fenetre ni module graphique : il recoit le
         * registre et va chercher lui-meme. C'est ce qui lui permet de suivre
         * une bascule de vendor sans que la borne ait a le prevenir.
         */
        void play(IAppModule *module) {
            quit();

            /* DETENTEUR avant tout : _app est alloue par cette dll et sa
             * vtable y vit. Sans ce compteur, Reconcile() fermerait la
             * bibliotheque sous une application vivante. C'est la meme regle
             * que pour les vendors, elle avait simplement ete oubliee ici -
             * personne ne s'en apercevait tant qu'on ne pouvait pas
             * decharger un jeu. */
            module->acquire();
            _running = module;
            _app = module->createApp(_modules);

            if (!_app) {
                _status = std::string(module->name()) + " : createApp a echoue";
                quit();
            }
        }

        /** @brief Arrete le jeu en cours. La dll qui a alloue libere. */
        void quit() {
            if (!_running)
                return;

            /* L'ordre, comme partout : detruire pendant que la dll vit, et
             * relacher seulement apres. Entre les deux elle pourrait fermer. */
            if (_app)
                _running->deleteApp(_app);
            _running->release();
            _app = nullptr;
            _running = nullptr;
        }

        /**
         * @brief Relit le dossier : ce qui est apparu depuis est charge.
         *
         * C'est tout le "chargement a chaud" dont une borne a besoin : tu
         * deposes une dll dans lib/, tu appuies sur R, elle est dans le menu.
         * Pas de navigateur de fichiers, pas de pwd, pas de ls.
         *
         * Load() refuse deja une cle presente, donc rien a filtrer ici - un
         * second passage ne recharge pas ce qui est deja la.
         */
        void reload() {
            const size_t before = _modules.GetKeys().size();
            const size_t seen = scan();
            const size_t added = _modules.GetKeys().size() - before;

            _status = std::to_string(added) + " ajoutee(s) sur " +
                      std::to_string(seen) + " fichier(s) lu(s)";
        }

        /**
         * @brief Condamne la bibliotheque selectionnee.
         *
         * Rien ne ferme ici. Unload() pose seulement le drapeau sur chaque
         * module de la colonne ; c'est evacuate() qui lachera, et
         * Reconcile() qui fermera - deux etapes plus loin, au meme tick.
         */
        void unload() {
            /* Un jeu est un module comme un autre : meme condamnation, meme
             * attente, meme fermeture. Refuser sa colonne etait un cas
             * particulier sans justification. */
            const std::vector<Entry> list = entries();

            if (cursor() >= list.size())
                return;
            _modules.Unload(list[cursor()].key);
        }

        /**
         * @brief Si ma bibliotheque est condamnee : lacher, puis basculer.
         *
         * L'ordre est tout : release() detruit mes objets et rend le verrou,
         * et seulement apres je vais chercher un remplacant. Prendre le
         * nouveau d'abord ouvrirait une seconde fenetre - et sous raylib,
         * createWindow() rendrait nullptr.
         *
         * Sans remplacant, la borne RESTE, aveugle. Elle s'arretait, du
         * temps ou une borne sans fenetre etait une borne que plus personne
         * ne pouvait piloter. Le terminal a change ca, et use() le disait
         * deja - un chemin sur deux avait ete mis a jour, pas l'autre.
         */
        void evacuate() {
            if (!_using || !_using->mustClose())
                return;

            release();

            for (IGraphic2Module *module : GetGraphicModules())
                if (!module->mustClose())
                    return use(module);

            _status = "plus aucune bibliotheque : reload, puis set graphic <nom>";
            std::printf("%s\n", _status.c_str());
            std::fflush(stdout);
        }

        /**
         * @brief Bascule sur ce module.
         *
         * Prend le MODULE, pas un indice : la liste change des qu'on charge
         * ou decharge, et un indice y designerait alors quelqu'un d'autre.
         * C'est ce qui faisait detruire une fenetre raylib en passant par
         * sfml apres un rechargement.
         */
        void use(IGraphic2Module *module) {
            if (!module || module == _using)
                return;

            /* QUATRE TEMPS, et l'ordre est tout. L'ancien doit etre
             * ENTIEREMENT eteint avant que le nouveau ne s'allume : ils se
             * partagent des globales de processus. Un contexte OpenGL entre
             * raylib et sfml ; et sous sdl2-compat, un unique libSDL3 pour
             * sdl2_impl ET sdl3_impl. Deux vivants en meme temps, c'est l'un
             * qui defait l'initialisation de l'autre.
             *
             * 1. PLUS PERSONNE en service. Les invites lisent nullptr, et
             *    follow() sait alors lacher SANS se rebrancher. C'est ce qui
             *    empeche un invite d'allumer le nouveau vendor pendant que
             *    l'ancien respire encore - et, sous raylib, de rafler au
             *    passage l'unique fenetre avant l'hote.
             *
             * 2. les invites lachent, pendant que le contexte de l'ANCIEN
             *    est encore vivant : c'est la seule fenetre de tir. Leur
             *    etat ne touche pas au vendor, il survit.
             *
             * 3. l'hote rend tout. L'ancien ferme ici ses sous-systemes, et
             *    plus rien de lui ne tourne.
             *
             * 4. SEULEMENT MAINTENANT on declare le nouveau et on l'allume.
             *    Les invites se rebrancheront d'eux-memes au tick suivant,
             *    une fois que l'hote aura sa fenetre. */
            select(GRAPHIC2, nullptr);

            if (_app)
                _app->update();

            release();

            select(GRAPHIC2, module);
            module->acquire();
            _using = module;
            _window = module->createWindow(WIDTH, HEIGHT, "P-E-R-R-Y arcade");

            /* Un invite a pu prendre l'unique fenetre du vendor en se
             * rebranchant a l'etape 2, juste avant nous. L'hote passe avant :
             * sans fenetre il ne peut plus meme recevoir echap, alors que le
             * jeu, lui, sait continuer sans rien voir. On l'arrete, et on
             * redemande. */
            if (!_window && _app) {
                _status = std::string(_running->name()) + " arrete : " +
                          module->name() + " ne donne qu'une fenetre";
                quit();
                _window = module->createWindow(WIDTH, HEIGHT, "P-E-R-R-Y arcade");
            }

            if (!_window) {
                /* On reste. Avant le terminal, une borne aveugle etait une
                 * borne que plus personne ne pouvait piloter, donc on
                 * s'arretait ; maintenant "set graphic <nom>" la ramene. */
                _status = std::string(module->name()) + " ne donne pas de fenetre";
                std::printf("%s ne donne pas de fenetre\n", module->name());
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
         * LE MODULE SEUL DECIDE, pas la fenetre. Un vendor qui a refuse d'en
         * ouvrir une avait quand meme ete acquis : exiger _window ici
         * laissait son compteur a un pour toujours, et Reconcile() ne
         * pouvait plus jamais fermer sa dll. Chaque tentative en ajoutait
         * une - trois essais, trois detenteurs fantomes.
         *
         * C'est la garde qu'a deja quit() de son cote : le detenteur se
         * lache parce qu'il tient, pas parce que ca a marche.
         */
        void release() {
            if (!_using)
                return;

            /* Le jeu emprunte cette fenetre : il doit avoir lache AVANT
             * qu'on la detruise. On le previent en le laissant voir la
             * condamnation, il se rebranche seul au tick suivant. */

            IGraphic2Module *module = _using;

            /* Chacun teste : sur le chemin du refus il n'y a rien a
             * rendre, et un vendor n'a pas a encaisser un nul. */
            if (_hint)     module->deleteText(_hint);
            if (_body)     module->deleteText(_body);
            if (_title)    module->deleteText(_title);
            if (_font)     module->deleteFont(_font);
            if (_keyboard) module->deleteKeyboard(_keyboard);
            if (_window)   module->deleteWindow(_window);

            /* Le relachement vient APRES les destructions, jamais avant :
             * entre les deux, sa dll pourrait se fermer. */
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
         * @brief Une commande tapee au terminal.
         *
         * Meme vocabulaire que le menu, meme effet : ce sont les memes
         * fonctions qui sont appelees. Le terminal n'est pas un second
         * chemin, c'est une seconde entree sur le meme.
         */
        void command(const std::string &line) {
            const std::vector<std::string> words = split(line);

            if (words.empty())
                return;

            const std::string &verb = words[0];
            const std::string what = words.size() > 1 ? words[1] : "";
            const std::string name = words.size() > 2 ? words[2] : "";

            if (verb == "help")   return usage();
            if (verb == "quit" || verb == "exit") return stop();
            if (verb == "reload") { reload(); return say(_status); }

            if (verb == "list" && what == "graphics") return show(GetGraphics());
            if (verb == "list" && what == "games")    return show(GetGames());
            if (verb == "list" && (what == "libs" || what == "libraries"))
                return showLibraries();
            if (verb == "list" && what == "others")   return show(GetOthers());

            if (verb == "get" && what == "graphic")
                return say(_using ? _using->name() : "(aucune)");
            if (verb == "get" && what == "game")
                return say(_running ? _running->name() : "(aucun)");

            if (verb == "set" && what == "graphic")   return setGraphic(name);
            if (verb == "set" && what == "game")      return setGame(name);

            if (verb == "unset" && what == "graphic") { release(); return say("aucune bibliotheque en service"); }
            if (verb == "unset" && what == "game")    { quit(); return say("aucun jeu"); }

            if (verb == "unload") return doUnload(what);

            /* Tout autre contrat decouvert au chargement - "audio" en est un.
             * Les quatre memes verbes, sans une ligne par famille. */
            if (known(what)) {
                if (verb == "list") return show(of(what), what);
                if (verb == "get") {
                    IModule *inService = current(what);

                    return say(inService ? inService->name() : "(aucun)");
                }
                if (verb == "set")   return setModule(what, name);
                if (verb == "unset") {
                    select(what, nullptr);
                    return say("aucun " + what + " en service");
                }
            }

            say("commande inconnue : " + verb + "   (help)");
        }

        /** @brief Le menu : deux colonnes, un curseur dans l'une des deux. */
        /**
         * @brief Le menu, ou un simple bandeau si un jeu tourne.
         *
         * La borne s'efface pendant la partie : elle garde la boucle, la
         * fenetre et le clavier, mais laisse l'ecran au jeu. C'est ce que
         * donnerait une seconde fenetre, sans les deux choses qui la rendent
         * impossible - raylib n'en ouvre qu'une, et sous sfml seule celle qui
         * a le focus recoit le clavier.
         */
        void draw() {
            _title->setFont(_font);
            _title->setFontSize(28);
            _title->setTextColor({255, 205, 80, 255});
            _title->setPosition({40.f, 30.f});
            _title->setText("P-E-R-R-Y  arcade");
            _window->drawText(_title);

            /* COTE A COTE, une colonne par contrat. Les fleches gauche et
             * droite passent de l'une a l'autre : empilees verticalement,
             * elles demandaient au joueur de deviner ce que la direction
             * faisait. */
            const std::vector<std::string> all = columns();
            const float width = (WIDTH - 2 * MARGIN) / (all.empty() ? 1 : all.size());

            for (size_t c = 0; c < all.size(); c++) {
                const bool here = (c == _column);
                const std::vector<Entry> list = entriesOf(all[c]);
                std::string text = (here ? "> " : "  ") + heading(all[c]) + "\n\n";

                if (list.empty())
                    text += "   (aucun)\n";
                for (size_t i = 0; i < list.size(); i++)
                    text += line(here && i == _cursors[all[c]],
                                 live(all[c], list[i]), list[i]);

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

        /** @brief Le titre d'une colonne, en majuscules. */
        static std::string heading(const std::string &type) {
            std::string title = type;

            for (char &c : title)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return title;
        }

        /** @brief Cette entree est-elle celle en service pour son contrat ? */
        bool live(const std::string &type, const Entry &entry) {
            if (type == IAppModule::contract)
                return _running && _running->name() == entry.name;

            /* Une bibliotheque est marquee quand c'est d'elle que vient la
             * fenetre : c'est le seul lien que la borne ait avec une dll. */
            if (type == LIBS)
                return _using && keyOf(_using) == entry.key;

            /* Colonne fusionnee : chaque entree repond de SON contrat, pas
             * de celui de la colonne, qui n'en est pas un. */
            IModule *inService = current(type == OTHERS ? entry.type : type);

            return inService && inService->name() == entry.name;
        }

        /** @brief La bibliotheque d'ou vient ce module, "" si introuvable. */
        std::string keyOf(IModule *module) {
            for (const std::string &key : _modules.GetKeys())
                if (_modules.Get(module->type(), key) == module)
                    return key;
            return "";
        }

        /**
         * @brief Une entree du menu.
         *
         * DEUX reperes, parce que ce sont deux choses : le curseur dit ou tu
         * es, l'etoile dit qui a fabrique la fenetre que tu regardes. Ils se
         * separent des qu'on charge ou decharge.
         */
        static std::string line(bool selected, bool active, const Entry &entry) {
            return std::string(selected ? " > " : "   ") + entry.name +
                   "   (" + entry.type + ")" + (active ? "  *en service" : "") + "\n";
        }

        /** @brief Une ligne de la table, mise en forme pour l'affichage. */
        std::vector<Entry> of(const std::string &type) {
            std::vector<Entry> found;

            for (const std::string &key : _modules.GetKeys())
                if (IModule *module = _modules.Get(type, key))
                    found.push_back({key, module->name(), module->type()});
            return found;
        }

        /**
         * @brief Charge chaque bibliotheque du dossier, sous une cle = son nom.
         *
         * L'extension retenue vient de SharedLibrary::extension(), choisie
         * par macro a la compilation : .dylib, .so ou .dll. Une liste ecrite
         * ici accepterait des fichiers que la plateforme courante ne saurait
         * pas ouvrir, et il faudrait la tenir a jour a deux endroits.
         *
         * Une bibliotheque qui ne s'ouvre pas est ignoree, comme celle qui
         * n'exporte pas getModules() : un dossier de greffons finit toujours
         * par contenir un intrus, ce n'est pas une raison pour refuser de
         * demarrer.
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

        static constexpr float MARGIN = 40.f;
        static constexpr int32_t WIDTH = 900;
        static constexpr int32_t HEIGHT = 560;

        Console _console;
        /* ---- ce que LA BORNE affiche comme en service ------------------ *
         *
         * Son propre etat de menu, plus celui de la table : un jeu ne suit
         * plus ce choix, il lit GetAllByType() et prend ce qu'il veut. Ce
         * qui est ici ne concerne donc que l'affichage de la borne et la
         * fenetre qu'elle ouvre pour elle-meme. */

        /** @brief Le module que la borne affiche en service, ou nullptr. */
        IModule *current(const std::string &type) const {
            const auto found = _current.find(type);

            return found == _current.end() ? nullptr : found->second;
        }

        /** @brief Declare celui en service. nullptr pour n'en avoir aucun. */
        void select(const std::string &type, IModule *module) {
            if (module)
                _current[type] = module;
            else
                _current.erase(type);
        }

        /** @brief Oublie ce qui a ete ferme : la table ne le connait plus. */
        void forget() {
            for (auto it = _current.begin(); it != _current.end(); ) {
                const std::vector<std::string> &keys = _modules.GetKeys();
                bool alive = false;

                for (const std::string &key : keys)
                    if (_modules.Get(it->first, key) == it->second)
                        alive = true;
                it = alive ? std::next(it) : _current.erase(it);
            }
        }

        std::map<std::string, IModule *> _current;

        IModuleManager _modules;
        std::string _libraries;
        std::string _assets;
        std::string _status;   ///< resultat du dernier R, affiche en bas

        /* Un curseur par contrat, cree a la demande : une famille de
         * modules apparue au dernier R a le sien sans rien declarer. */
        std::map<std::string, size_t> _cursors;
        size_t _column = 0;

        /* Fabriques par le vendor courant, donc detruits par lui avant tout
         * changement. Ils ne survivent pas a un release(). */
        /* Le module EN COURS D'USAGE, distinct du curseur du menu. Deux
         * notions, deux variables : _graphic dit ce qui est surligne,
         * _using dit qui a fabrique la fenetre. */
        IAppModule *_running = nullptr;   ///< le module du jeu lance
        IApp *_app = nullptr;             ///< le jeu lui-meme

        IGraphic2Module *_using = nullptr;

        graphic::IWindow2 *_window = nullptr;
        graphic::IKeyboard *_keyboard = nullptr;
        graphic::IFont *_font = nullptr;
        graphic::IText *_title = nullptr;
        graphic::IText *_body = nullptr;
        graphic::IText *_hint = nullptr;
};

#endif /* !ARCADECORE_HPP_ */
