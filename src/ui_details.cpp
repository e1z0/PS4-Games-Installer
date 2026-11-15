#include "ui.hpp"
#include "metadata.hpp"
#include <algorithm>
#include <map>

namespace psgi {
void Ui::openDetails(Screen from) {
    detailReturn=from; detailScroll=detailIndex=mediaIndex=0;
    detailsExpanded=technicalExpanded=false; screen=Screen::Details;
}
std::vector<DetailRow> Ui::detailRows() const {
    std::vector<DetailRow> rows;
    auto add=[&](const std::vector<std::pair<std::string,std::string>>& fields) {
        for(const auto& field:fields) {
            std::string value=field.second, key=field.first;
            for(auto& c:value) if(c=='\r'||c=='\t')c=' ';
            for(auto& c:key) if(static_cast<unsigned char>(c)<32)c=' ';
            size_t start=0; bool first=true;
            do {
                auto end=value.find('\n',start);
                auto line=value.substr(start,end==std::string::npos ? end : end-start);
                do {
                    const size_t width=first ? 72-std::min<size_t>(key.size()+2,45) : 72;
                    size_t cut=line.size();
                    if(cut>width) {
                        cut=line.rfind(' ',width);
                        if(cut==std::string::npos||cut<width/2)cut=width;
                        while(cut&&(static_cast<unsigned char>(line[cut])&0xc0)==0x80)--cut;
                        if(!cut)cut=width;
                    }
                    rows.push_back({first ? key : "",line.substr(0,cut),0}); first=false;
                    if(cut>=line.size())break;
                    line.erase(0,cut+(line[cut]==' ' ? 1 : 0));
                } while(!line.empty());
                if(end==std::string::npos)break;
                start=end+1;
            } while(start<=value.size());
        }
    };
    add(entryDetails(pending));
    if(!pending.igdb.screenshots.empty())rows.push_back({"Screenshots",std::to_string(pending.igdb.screenshots.size())+" images  >",3});
    if(!pending.igdb.videos.empty())rows.push_back({"Videos",std::to_string(pending.igdb.videos.size())+" videos  >",4});
    rows.push_back({"Details",detailsExpanded ? "[-] Collapse" : "[+] Expand",1});
    if(detailsExpanded)add(entryAdditionalDetails(pending));
    rows.push_back({"Tech. details",technicalExpanded ? "[-] Collapse" : "[+] Expand",2});
    if(technicalExpanded)add(entryTechnicalDetails(pending));
    return rows;
}
std::vector<std::string> Ui::details() const {
    std::vector<std::string> result;
    for(const auto& row:detailRows())result.push_back((row.key.empty() ? "" : row.key+": ")+row.value);
    return result;
}
Entry Ui::screenshot() const {
    Entry image;
    if(!pending.igdb.screenshots.empty()) image.coverUrl=pending.igdb.screenshots[std::min(mediaIndex,pending.igdb.screenshots.size()-1)].url;
    return image;
}
std::vector<std::string> Ui::videoRows() const {
    std::vector<std::string> result;
    for(const auto& video:pending.igdb.videos)result.push_back(video.name);
    return result;
}
bool Ui::handleDetails(Button button) {
    if(screen==Screen::Screenshots||screen==Screen::Videos) {
        if(button==Button::Circle){screen=mediaReturn;return true;}
        const auto count=screen==Screen::Screenshots ? pending.igdb.screenshots.size() : pending.igdb.videos.size();
        if(!count){screen=mediaReturn;return true;}
        auto direction=button;
        if(button==Button::Left||button==Button::L1)direction=Button::Up;
        if(button==Button::Right||button==Button::R1)direction=Button::Down;
        moveMenuSelection(direction,mediaIndex,count);
        if(screen==Screen::Screenshots&&button==Button::Triangle)commands.push_back({CommandType::RefreshArtwork,screenshot(),1,""});
        if(screen==Screen::Videos&&button==Button::Cross)commands.push_back({CommandType::OpenMedia,{},mediaIndex,pending.igdb.videos[mediaIndex].url});
        return true;
    }
    if(screen!=Screen::Details)return false;
    if(button==Button::Circle){screen=detailReturn;return true;}
    if((button==Button::Triangle&&!pending.igdb.screenshots.empty())||(button==Button::Square&&!pending.igdb.videos.empty())) {
        screen=button==Button::Triangle ? Screen::Screenshots : Screen::Videos;mediaIndex=0;mediaReturn=Screen::Details;return true;
    }
    const auto rows=detailRows();moveMenuSelection(button,detailIndex,rows.size());
    if(button==Button::Cross) {
        switch(rows[detailIndex].action) {
            case 1: detailsExpanded=!detailsExpanded;break;
            case 2: technicalExpanded=!technicalExpanded;break;
            case 3: screen=Screen::Screenshots;mediaIndex=0;mediaReturn=Screen::Details;break;
            case 4: screen=Screen::Videos;mediaIndex=0;mediaReturn=Screen::Details;break;
        }
    }
    if(detailIndex<detailScroll)detailScroll=detailIndex;
    if(detailIndex>=detailScroll+12)detailScroll=detailIndex-11;
    return true;
}
std::vector<std::string> Ui::genres() const { return genreNames; }
bool Ui::hasServerFavorites() const {
    return std::any_of(available.begin(), available.end(), [](const Entry& entry) {
        return entry.serverFavorite && packageKind(entry) == 0;
    });
}
std::vector<SidebarRow> Ui::sidebarRows() const {
    const auto choices=genres();
    std::vector<SidebarRow> rows{{"Installed","",0},{"Available"+(choices.empty() ? std::string() : genresExpanded ? " [-]" : " [+]"),"",1}};
    if(genresExpanded)for(const auto& genre:choices)rows.push_back({genre,genre,1});
    if(hasServerFavorites()) rows.push_back({"Server favorites","",3});
    rows.push_back({"Updates","",2});
    return rows;
}
size_t Ui::activeSidebarIndex() const {
    const auto rows=sidebarRows();
    for(size_t i=0;i<rows.size();++i)if(rows[i].kind==(serverFavoritesOnly ? 3 : updatesOnly ? 2 : unsigned(library))&&rows[i].genre==(library==Library::Available&&!updatesOnly&&!serverFavoritesOnly ? remoteGenre : ""))return i;
    return size_t(library);
}
}
