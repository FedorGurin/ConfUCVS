#include "domParser.h"


#include "unitNode.h"
#include "connectorNode.h"


#include "wireNode.h"
#include "systemNode.h"
#include "pinNode.h"


#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QMessageBox>
#include <QTextCodec>
#include <QStringList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

//! путь к сохранению файлов в формате GraphViz
const char* pathToParsedGV =  "/csv/parsed/curcuit/gv/";

DomParser::DomParser(QObject *parent):QObject(parent)
{
    // дерево с описательной частью данных
    dataNodes           = nullptr;
    // корень для дерева с описанием данных
    rootItemData        = nullptr;
    listRootItemNode.clear();
    fileLog.setFileName("./csv/log/log.txt");
    bool fileOpen = fileLog.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen)
    {
        outLog.setDevice(&fileLog);
         outLog.setCodec("CP1251");
    }

//    templateChStr = " arTable.setCh(&(t->chTable->ch[AR_OUT_CO2010])); \
//            arTable.setAddr(E_NODE_CV, 1); \
//            arTable.setChId(E_CH_AR,13,E_CH_IO_OUTPUT); \
//            arTable << 0350  ; \
//            arTable.setProp(LayerArinc::KBs_50,LayerArinc::REV_RTM3,LayerArinc::ALWAYS); ";
}
void DomParser::exportTable()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenChTable = &DomParser::genChTable;
    saveDataToCVS("parsed/export/genChTable"   ,rootItemData,f_saveGenChTable);


}
void DomParser::genChTable(Node* startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode* > (startNode);
        for(auto j:unit->unknownInf)
        {
            QString strSource;
            QString strIo;
            QString bitrate = "-";
            QString typeCh;
            int iCh;
            if(j->ch.skip == 1)
                continue;
            if(j->ch.io == 1 )
                strIo="выдача";
            else
                strIo="прием";

            if(j->ch.typeNode == "ARM_IM")
            {
                if(j->ch.type == "E_CH_AR")
                {

                    uint8_t numA = j->ch.id / 48;
                    iCh = j->ch.id - numA * 48 + 1;

                    if(j->ch.id > 47)
                        strSource = "KK2_AR_";
                    else
                        strSource = "KK1_AR_";

                     bitrate = j->ch.bitrate + " Кбит/с";
                     typeCh = "Arinc-429";
                }else if(j->ch.type == "E_CH_RK")
                {
                    uint8_t numA = j->ch.id / 48;
                    iCh = j->ch.id - numA * 48  + 1 ;

                    if(j->ch.id > 47)
                        strSource = "KK2_RK_";
                    else
                        strSource = "KK1_RK_";
                      typeCh = "РК (" + j->ch.typeRK + ")";
                }
                if(j->ch.io == 1)
                    strSource +="OUT";
                else
                    strSource +="IN";



            }else if (j->ch.typeNode == "ARM_OVO")
            {
                iCh = j->ch.id + 1;
                if(j->ch.type == "E_CH_RK")
                {
                    strSource = "KK_OVO_RK";
                    typeCh = "РК (" + j->ch.typeRK + ")";
                }else if(j->ch.type == "E_CH_IP")
                {
                    strSource = "KK_OVO_IP";
                    typeCh = "В";
                }else if(j->ch.type == "E_CH_DAC")
                {
                    strSource = "KK_OVO_DAC";
                    typeCh = "В";
                }else if(j->ch.type == "E_CH_ITP")
                {
                    strSource = "KK_OVO_ITP";
                    typeCh = "ГрадС";
                }else if(j->ch.type == "E_CH_IR")
                {
                    strSource = "KK_OVO_IR";
                    typeCh = "Ом";
                }else if(j->ch.type == "E_CH_GEN_NU")
                {
                    strSource = "KK_OVO_FREQ";
                    typeCh = "Гц";
                }else if(j->ch.type == "E_CH_GEN_U")
                {
                    strSource = "KK_OVO_GEN";
                    typeCh = "~В";
                }

            }

            out<<iCh<<";"<<strSource<<";"<<bitrate<<";"<<typeCh<<";"<<strIo<<";"<<j->ch.displayName<<";"<<unit->displayName<<"\n";
        }
    }
    for(auto i:startNode->child)
    {
        genChTable(i,out);
    }
}

void DomParser::loadDataPIC(QString nameDir)
{
    bool curOpenFile = false;

    QStringList filtres;
    QJsonParseError jsonError;
    filtres<<"*.json";
    QDir dir("./csv"+nameDir);

    QFileInfoList fileList = dir.entryInfoList(filtres, QDir::Files);

    for(auto &f:fileList)
    {
        QFile file(f.absoluteFilePath());

        curOpenFile = file.open(QIODevice::ReadOnly | QIODevice::Text);
        if(curOpenFile == true)
        {
            QByteArray array = file.readAll();
            QJsonDocument loadDoc = QJsonDocument::fromJson(array,&jsonError);
            if(loadDoc.isNull() == true)
            {
                QMessageBox msgBox;
                msgBox.setText(file.fileName() + "-" +jsonError.errorString());
                msgBox.exec();


            }
            QJsonObject objJson = loadDoc.object();

            if(objJson.contains("systems") && objJson["systems"].isArray())
            {
                QJsonArray sysArray = objJson["systems"].toArray();
                for(int k = 0; k< sysArray.size(); k++)
                {
                    QJsonObject kArray = sysArray[k].toObject();
                    QString name = kArray["idSys"].toString();
                    // нужно найти блок с соотвествующим идентификатором
                    UnitNode* unit = static_cast<UnitNode* > (findNodeByIdName(name,rootItemData,Node::E_UNIT));
                    if(unit == nullptr)
                        continue;
                    if(objJson.contains("channels") && objJson["channels"].isArray())
                    {
                        QJsonArray objArray = objJson["channels"].toArray();
                        for(int i = 0;i < objArray.size();i++)
                        {
                            QJsonObject iArray = objArray[i].toObject();
                            unit->unknownInf.append(new InterfaceNode(iArray));
                        }
                        //qDebug()<<"size ="<<objJson.size()<<"\n";
                    }
                }
            }

                    //parseData(obj,rootNode);
                    /*for(auto &item:obj)
                    {
                        QJsonObject  t = item.toObject();
                        //QJsonValue::Type type = t.type();

                        qDebug("Hello\n");

                    }*/
        }
    }
}
void DomParser::genChEnum(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.skip == 1)
                continue;

            if(j->ch.type == "E_CH_AR")
            {
                j->ch.enumStr = "E_AR_" + j->ch.idName;

//                if(j->ch.io == 1)
//                    j->ch.enumStr += "OUT_";
//                else
//                    j->ch.enumStr += "IN_";

//                j->ch.enumStr += j->ch.idName;
//                if(j->ch.idConnectedUnit != "-")
//                    j->ch.enumStr += "_" + j->ch.idConnectedUnit;


                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;

            }else if(j->ch.type == "E_CH_RK")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_RK_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }
            else if(j->ch.type == "E_CH_IR")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_IR_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }else if(j->ch.type == "E_CH_DAC")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_DAC_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }else if(j->ch.type == "E_CH_IP")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_IP_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }else if(j->ch.type == "E_CH_ITP")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_ITP_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }else if(j->ch.type == "E_CH_GEN_NU")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_GEN_NU_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }
            else if(j->ch.type == "E_CH_GEN_U")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                j->ch.enumStr = "E_GEN_" + j->ch.idName.toUpper();
                out<<"\t"<<j->ch.enumStr<<" = "<< indexEnum <<", \n";
                indexEnum++;
            }


        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genChEnum(i,out);
    }

}
void DomParser::genCh(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.skip == 1)
                continue;
            if(j->ch.type == "E_CH_AR")
            {
                out << "arTable.setCh(&(t->chTable->ch[" + j->ch.enumStr+"])); \n";
                out << "arTable.setAddr(E_NODE_CV, 1); \n";
                out << "arTable.setChId(E_CH_AR," + QString::number(j->ch.id) + "," + j->ch.ioStr+"); \n";

                out << "arTable" ;
                for(auto k:j->ch.addrs)
                {
                   QString addr8 = QString::number(k,8);
                   out<<" << 0" + QString::number(k,8);
                }
                out<<";\n";
                if(j->ch.idConnectedUnit.isEmpty() == false)
                    out<< "arTable.setChConnected(E_AR_" << j->ch.idConnectedUnit<<");\n";
                out <<"arTable.setProp(LayerArinc::";
                if(j->ch.bitrate == "100")
                    out<<"KBs_100";
                else if (j->ch.bitrate == "200")
                    out<<"KBs_200";
                else if (j->ch.bitrate == "12.5")
                    out<<"KBs_12_5";
                else
                    out<<"KBs_50";
                out<<",LayerArinc::REV_RTM3,LayerArinc::ALWAYS,";
                if(j->ch.period == "8")
                    out<<"LayerArinc::INTER_8T";
                else
                    out<<"LayerArinc::INTER_4T";
                out<<"); \n\n";
            }else if(j->ch.type == "E_CH_RK")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                if(j->ch.typeNode == "ARM_OVO")
                {
                    out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                }
                else
                {
                    out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_CV; \n";
                }

                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                if(j->ch.idConnectedUnit.isEmpty() == false)
                    out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numChConnected = " << "E_RK_"<<j->ch.idConnectedUnit<<";\n";
                out<<"\n";
            }else if(j->ch.type == "E_CH_IR")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "ir = (TDesIR*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";
                out << "ir->max = 240; \n";
                out << "ir->min = 30; \n";
                out << "ir->value = ir->min; \n";
                out<<"\n";
            }else if(j->ch.type == "E_CH_IP")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "ip = (TDesIP*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";

                out << "ip->max = 10; \n";
                out << "ip->value = 0; \n";

                out<<"\n";
            }else if(j->ch.type == "E_CH_ITP")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "itp = (TDesITP*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";
                out << "itp->type = 0; \n";
                out << "itp->max = 45; \n";
                out << "itp->value = 0; \n";
                out<<"\n";
            }else if(j->ch.type == "E_CH_DAC")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "dac = (TDesDAC*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";
                out << "dac->volt  = 0; \n";
                out<<"\n";

            }else if(j->ch.type == "E_CH_GEN_NU")
            {

                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "gen_nu = (TDesGen_NU*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";
                out << "gen_nu->freq  = 400; \n";
                out<<"\n";

            }
            else if(j->ch.type == "E_CH_GEN_U")
            {
                if(j->ch.enumStr.isEmpty())
                    continue;
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeNode = "<<" E_NODE_PV; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].idNode = "<<" 1; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].typeCh = "<< j->ch.type<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numAdapter = "<<" 0; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.numCh = "<< QString::number(j->ch.id)<<"; \n";
                out << "t->chTable->ch[" << j->ch.enumStr<<"].setting.ioCh = "<< j->ch.ioStr<<"; \n";
                out << "gen_u = (TDesGen_U*) (t->chTable->ch[" << j->ch.enumStr<<"].desData); \n";
                out << "gen_u->u1  = 110; \n";
                out << "gen_u->u2  = 110; \n";
                out << "gen_u->u3  = 110; \n";
                out << "gen_u->freq  = 400; \n";
                out<<"\n";

            }
        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genCh(i,out);
    }

}
void DomParser::genEnum_title(Node* rootNode, QTextStream& out)
{
    out<<tr("enum E_CH {")<<"\n";
    indexEnum = 0;
    genChEnum(rootNode,out);
    out<<tr("E_CH_MAX\n");
    out<<tr("};");
}

void DomParser::saveCh()
{
    //! сохранение данных в cvs
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCh = &DomParser::genCh;
    saveDataToCVS("parsed/export/genCh"   ,rootItemData,f_saveGenCh);
}

void DomParser::saveChEnum()
{
    //! сохранение данных в cvs
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCpp = &DomParser::genEnum_title;
    saveDataToCVS("parsed/export/genEnumCh"   ,rootItemData,f_saveGenCpp);
}
void DomParser::genPackEnum(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {

            for(auto &k:j->params)
            {
               QString m;

               if(k.csr.isEmpty() == true && k.cmr.isEmpty() == false)
               {
                   float cmr = k.cmr.toFloat();
                   int bit = k.hiBit.toInt() - k.lowBit.toInt();
                   if(k.sign == 1)
                       bit--;
                   k.csr = QString::number((1<<bit) * cmr);
               }
               else if(k.csr.isEmpty() == true)
               {
                   k.csr = "0";
               }

               m = k.csr;
               m.replace(",","_");
               m.replace(".","_");
               QString strEnum = "E_P_AR_H" + k.hiBit + "_L" +
                       k.lowBit + "_S" + QString::number(k.s) + "_M" + m;
               if(packCode.enumStr.contains(strEnum) == false)

               {
                   QString m1 = k.csr;
                   if(k.s == 1)
                       m1+="* 2.0";

                   m1.replace(",",".");
                   QString codeStr = "addPackToTable(" + strEnum + ",TO_AR("+ k.hiBit+"),TO_AR(" + k.lowBit + "),"+  QString::number(k.s) + "," + m1+ ");";
                   packCode.codeStr.append(codeStr);
                   packCode.enumStr.append(strEnum);

                   out<<strEnum<<",\n";
               }
               k.enumCh     = "E_AR_" + j->ch.idName;
               k.enumPack   = strEnum;
               QStringList p;
               if(k.label.isEmpty() == true)
                   p = k.idName.split(".");
               else
                   p = k.label.split(".");
               int l = 0;
               for(l = 0 ;l < p.size();l++)
               {
                   if(p[l].contains("Input",Qt::CaseInsensitive) || p[l].contains("Output",Qt::CaseInsensitive))
                   {
                       break;
                   }
               }
               QString name ="E";//p.back().toUpper();
               if(p.size() >2)
               {
                   for(int k = l -1; k < p.size();k++)
                   {
                       p[k].replace("[","_");
                       p[k].replace("]","");

                       name += "_" + p[k].toUpper();
                   }
                   k.enumParam  = name;
               }
            }


        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genPackEnum(i,out);
    }

}
void DomParser::genParamEnum(Node* rootNode, QTextStream& out,QStringList &listStr)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.copyFrom.isEmpty())
            {
                for(auto &k:j->params)
                {

                    if(k.enumParam.isEmpty() == false)
                    {
                        if(listStr.contains(k.enumParam) == false)
                        {
                            listStr.append(k.enumParam);
                            out<<k.enumParam<<","<<"\n";
                        }
                    }
                }
            }
        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genParamEnum(i,out,listStr);
    }

}
void DomParser::genPackEnum_title(Node* rootNode, QTextStream& out)
{
    out<<tr("enum E_PACK {")<<"\n";

    genPackEnum(rootNode,out);
    out<<tr("};");
}
void DomParser::genParamEnum_title(Node* rootNode, QTextStream& out)
{
    out<<tr("enum E_PARAM {")<<"\n";

    QStringList listParam;
    genParamEnum(rootNode,out,listParam);
    out<<tr("};");
}
void DomParser::genPackTable(Node* rootNode, QTextStream& out)
{
    for(auto i:packCode.codeStr)
    {
        out<<i<<"\n";
    }
}
void DomParser::genParamTable(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.copyFrom.isEmpty())
            {
            for(auto k:j->params)
            {
                if(k.enumParam.isEmpty() == false)
                    out<<"addParamToTable(" << k.enumParam << "," <<k.enumCh <<"," << k.enumPack<<"," << "0"+QString::number(k.addr,8) <<");\n";
            }
            }
        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genParamTable(i,out);
    }
}
void DomParser::genPackingCode(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.type == "E_CH_AR")
            {
                if(j->ch.copyFrom.isEmpty())
                {
                    for(auto k:j->params)
                    {
                        k.idName.replace("Input","input");
                        int index = k.idName.indexOf("input",Qt::CaseInsensitive);
                        if(index == -1)
                        {
                            k.idName.replace("Output","output");
                            index = k.idName.indexOf("output",Qt::CaseInsensitive);

                        }
                    QString addr = k.idName.mid(index);
                    if(addr != "-" && addr.isEmpty() == false)
                        out<<"doHAL(" << k.enumCh <<","<< k.enumParam <<"," << addr <<");\n";
                    }
                }
                else
                {
                    out<<"copyChHAL(E_AR_" << j->ch.copyFrom<<", E_AR_" << j->ch.idName<<");\n";
                }
            }else if(j->ch.type == "E_CH_RK")
            {
                if(j->ch.skip == 1 && j->ch.idConnectedUnit.isEmpty() == true)
                    continue;
                if(j->ch.skip)
                    out<<"readHAL(E_RK_"<<j->ch.idConnectedUnit<<","<<" "<<");\n";
                else
                    out<<"doHAL(E_RK_"<<j->ch.idName<<","<<" "<<");\n";
            }else if(j->ch.type == "E_CH_DAC")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_DAC_"<<j->ch.idName<<","<<" "<<");\n";
            }else if(j->ch.type == "E_CH_IR")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_IR_"<<j->ch.idName<<","<<" "<<");\n";
            }else if(j->ch.type == "E_CH_IP")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_IP_"<<j->ch.idName<<","<<" "<<");\n";
            }else if(j->ch.type == "E_CH_ITP")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_ITP_"<<j->ch.idName<<","<<" "<<");\n";
            }else if(j->ch.type == "E_CH_GEN_NU")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_GEN_NU_"<<j->ch.idName<<","<<" "<<");\n";
            }
            else if(j->ch.type == "E_CH_GEN_U")
            {
                if(j->ch.idName.isEmpty())
                    continue;
                out<<"doHAL(E_GEN_"<<j->ch.idName<<","<<" "<<");\n";
            }
        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        genPackingCode(i,out);
    }
}
void DomParser::savePack()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCpp = &DomParser::genPackTable;
    saveDataToCVS("parsed/export/genPack"   ,rootItemData,f_saveGenCpp);
}
void DomParser::savePackingCode()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_savePackingCpp = &DomParser::genPackingCode;
    saveDataToCVS("parsed/export/genCodeForModel"   ,rootItemData,f_savePackingCpp);
}
void DomParser::saveParam()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCpp = &DomParser::genParamTable;
    saveDataToCVS("parsed/export/genParam"   ,rootItemData,f_saveGenCpp);
}
void DomParser::savePackEnum()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCpp = &DomParser::genPackEnum_title;
    saveDataToCVS("parsed/export/genEnumPack"   ,rootItemData,f_saveGenCpp);
}
void DomParser::saveParamEnum()
{
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveGenCpp = &DomParser::genParamEnum_title;
    saveDataToCVS("parsed/export/genEnumParam"   ,rootItemData,f_saveGenCpp);
}
void DomParser::saveRP(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.idName.isEmpty() || j->ch.idName == "-")
               continue;

            if(j->ch.type == "E_CH_AR")
                out<<j->ch.idName<< ";" << indexEnum <<"\n";
            indexEnum++;


        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        saveRP(i,out);
    }
}
void DomParser::saveRP_BD_Title(Node* rootNode, QTextStream& out)
{
    out<<tr("Название|Единицы измерения|Описание|Протокол|Адрес|Начальный бит|Конечный бит|Знаковое значение|Масштаб")<<"\n";

    indexEnum = 0;
    saveRP_BD(rootNode,out);
}
void DomParser::saveRP_BD(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode * unit = static_cast<UnitNode* > (rootNode);

        for(auto j : unit->unknownInf)
        {
            if(j->ch.idName.isEmpty())
                continue;

            if(j->ch.type == "E_CH_RK" && j->ch.idName.isEmpty() == false)
            {
                out<<j->ch.idName<<"|"<<"RK"<<"|" <<j->ch.idName<<"|"<<indexEnum<< "|||||\n";

            }
            else
            {
            for(auto k: j->params)
            {
                if(k.idName.isEmpty())
                    continue;
                QString sign = "0";
                QString cmr  = "1";
                if(k.sign == "да")
                    sign = "-1";


                if(k.cmr.isEmpty() && k.csr.isEmpty() == false)
                {
                    cmr = QString::number(k.csr.toFloat()/ pow(2,k.hiBit.toInt() - k.lowBit.toInt() -1));
                }else if(k.cmr.isEmpty() == false)
                {
                    cmr = k.cmr;
                }


                QString idName;

                QStringList idNames = k.idName.split(".", QString::SkipEmptyParts);
                //int index = k.idName.indexOf("Input",Qt::CaseInsensitive);
                //if(index == -1)
                //    index = k.idName.indexOf("Output",Qt::CaseInsensitive);
                //idName = k.idName.mid(index);
                bool isHas = false;
                for(int i = 0; i <idNames.size();i++)
                {
                    if(idNames[i].contains("input", Qt::CaseInsensitive) || idNames[i].contains("output", Qt::CaseInsensitive))
                    {
                        isHas = true;
                        idName += idNames[i-1];
                    }
                    if(isHas == true)
                        idName += "." + idNames[i];
                }
                out<<idName<<"|"<<k.units<<"|" <<k.fullName<<"|"<<indexEnum<<"|"
                   <<"0"+QString::number(k.addr,8)  <<"|"<<k.lowBit<<"|"<<k.hiBit<<"|"<< sign <<"|"<<cmr << "\n";

            }

            }
             indexEnum++;


        }
    }
    out.flush();
    for(auto i : rootNode->child)
    {
        saveRP_BD(i,out);
    }
}
void DomParser::saveForRP()
{
    indexEnum = 0;
    //! сохранение данных в cvs
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveRP = &DomParser::saveRP;
    saveDataToCVS("parsed/export/channels"   ,rootItemData,f_saveRP);

    //! сохранение данных в cvs
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveRP_BD = &DomParser::saveRP_BD_Title;
    saveDataToCVS("parsed/export/parameters"   ,rootItemData,f_saveRP_BD, QString("CP1251"));

}
void DomParser::loadTransitFile(QString nameDir)
{
    bool curOpenFile = false;

    QStringList filtres;
    QJsonParseError jsonError;
    filtres<<"*.json";
    QDir dir("./csv"+nameDir);

    QFileInfoList fileList = dir.entryInfoList(filtres, QDir::Files);

    for(auto &f:fileList)
    {
        QFile file(f.absoluteFilePath());

        curOpenFile = file.open(QIODevice::ReadOnly | QIODevice::Text);
        if(curOpenFile == true)
        {
            listTransitFromFile.clear();
            QByteArray array = file.readAll();
            QJsonDocument loadDoc = QJsonDocument::fromJson(array,&jsonError);
            QJsonObject objJson = loadDoc.object();
            if(objJson.contains("systems") && objJson["systems"].isArray())
            {
                QJsonArray sysArray = objJson["systems"].toArray();
                for(int k = 0; k< sysArray.size(); k++)
                {
                    QJsonObject kArray = sysArray[k].toObject();
                    TTransitRecord tempRecord;
                    tempRecord.nameSys1 = kArray["idSys"].toString();
                    if(kArray.contains("transitSys") && kArray["transitSys"].isArray())
                    {
                        QJsonArray trArray = kArray["transitSys"].toArray();
                        for(int i = 0;i < trArray.size();i++)
                        {
                            QJsonObject iArray = trArray[i].toObject();
                            tempRecord.nameTr.append(iArray["name"].toString());
                        }
                        QJsonArray sys2Array = kArray["idSys2"].toArray();
                        for(int i = 0;i < sys2Array.size();i++)
                        {
                            QJsonObject iArray = sys2Array[i].toObject();
                            tempRecord.namesSys2.append(iArray["name"].toString());
                        }
                    }
                    listTransitFromFile.append(tempRecord);
                }
            }

                    //parseData(obj,rootNode);
                    /*for(auto &item:obj)
                    {
                        QJsonObject  t = item.toObject();
                        //QJsonValue::Type type = t.type();

                        qDebug("Hello\n");

                    }*/
        }
    }
}
//void DomParser::parseData(const QJsonObject &element, GenericNode *parent)
//{
//    GenericNode *item = nullptr;

//    /*for(int i=0;i<element.size();i++)
//    {
//        node=element[i]
//    }*/
//   // QJsonObject::iterator first = element.end();
//    for(auto it= element.begin();it!=element.end();++it)
//    {
//        QString str = it.key();
////        for(auto &node1:node.toObject())
////        {
////            qDebug("Hello\n");

////        }
//        //QJsonObject j = node.toObject();

//        parseData((it.value()).toObject(),item);
//       /// if(i.tagName()        =="pmodule")    {item = new PModule   (ele,parent);}
////        else if(ele.tagName()   =="sio")        {item = new SIO       (ele,parent);}
////        else if(ele.tagName()   =="param")      {item = new Parameter (ele,parent);}
////        else if(ele.tagName()   =="group")      {item = new GroupLabel(ele,parent);}
////        else if(ele.tagName()   =="union")      {item = new Union     (ele,parent);}

////        if(item != nullptr)
////            parseData(ele,item);
////        if(ele.tagName() == "struct")
////        {
////            Structure *item = new Structure(ele,parent);
////            //! рекурсия в содержимое
////            parseData(ele,item);
////            item->addMassiveStruct();
////        }
////        item = nullptr;
////        ele = ele.nextSiblingElement();
//    }


//}

void DomParser::joingInterface(Node *startNode)
{

}

//! загрузка из
void DomParser::loadData(QString dir, EPropertySaveToGV type,bool isAutoGraph)
{
    bool okDesData = false;
    // создаем корень структуры
    rootItemData         = new Node;
    rootItemData->idName = "Ка-226";

    // указатели на функции
    std::function<void(DomParser&, QString,Node*)> f_parseData     = &DomParser::parseData;
    std::function<void(DomParser&, QString,Node*)> f_parseSpec     = &DomParser::parseLocation;
    std::function<void(DomParser&, QString,Node*)> f_parseTrans    = &DomParser::parseTransData;
    std::function<void(DomParser&, QString,Node*)> f_parseAlias    = &DomParser::parseAlias;
    std::function<void(DomParser&, QString,Node*)> f_parseLength    = &DomParser::parseLength;

    // функция сохранения всех соединений (проводов)
    std::function<void(DomParser&,Node *, QTextStream&)> f_saveNodeGV = nullptr;
    if(type == E_WIRES)
        f_saveNodeGV = &DomParser::saveNodeWiresGW;
    else if(type == E_INTERFACES)
        f_saveNodeGV = &DomParser::saveNodeInterfaceGW;
//    else if(type == E_CORDS)

    // открываем файлы со спецификацией
    okDesData = openFileDesData("./csv/spec",listRootItemNode,f_parseSpec );
    if(okDesData == false)
        return;

    // открываем файлы с данными соединений и блоков
    okDesData = openFileDesData("./csv" + dir,listRootItemNode,f_parseData );
    if(okDesData == false)
        return;

    // открываем файлы с трансформациями
    okDesData = openFileDesData("./csv/transform",listRootItemNode,f_parseTrans );
    if(okDesData == false)
        return;
    // открываем файлы с длинами
    okDesData = openFileDesData("./csv/route",listRootItemNode,f_parseLength );
    if(okDesData == false)
        return;

    // открываем файлы с псевдонимами
    okDesData = openFileDesData("./csv/alias",listRootItemNode,f_parseAlias );
    if(okDesData == false)
        return;

//    // открываем файлы с псевдонимами
//    okDesData = openFileDesData("./csv/route",listRootItemNode,f_parseAlias );
//    if(okDesData == false)
//        return;

    // реализация соединений
    for(auto i:listRootItemNode)
    {
        // осуществляем подключение проводов и клемм между собой
        connectWires    (i);
        // выдялем провода и клеммы одного интерфейса
        correctInterface(i);
        // корректируем название соединений
        correctWire     (i);        
        correctCoords(i);
        // сохранение связей и объектов в формате GraphViz согласно выбранным настройкам (провода, жгуты, интерфейсы)
        //saveForGraphviz (pathToParsedGV,i->idName,i,f_saveNodeGV);
    }
    for(auto i:listRootItemNode)
    {
        // объединение всех узлов в один граф
        for(auto j:i->child)
            mergeNodes(rootItemData, j);
    }


    // соединение всех проводов для уже объединенной модели
    connectWires    (rootItemData);

    // выделение интерфейсов в  уже объединенной модели
    correctInterface(rootItemData);
    correctWire     (rootItemData);
    correctCoords(rootItemData);
    // загрузить внутрение соединения
    loadInternalConnection();
    // объединение интерфейсов в зависимости от подключения
    joingInterface(rootItemData);
    fillGeometryUnit(rootItemData);

    // заполнение интtрфейсов
    checkConnectionInterfaces(rootItemData);
    // поиск соединений между локациями
    saveConnectionLocations(rootItemData,"CAB");
    if(isAutoGraph)
    {
        // сохранение данных в формате GraphViz
        saveForGraphviz(pathToParsedGV,rootItemData->idName,rootItemData,f_saveNodeGV);
    }

    // сохранение соединений в файл
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveWires = &DomParser::saveCSVConnection;
    saveDataToCVS("parsed/export/wires",rootItemData, f_saveWires);

    std::function<void(DomParser&, Node*,QTextStream&)> f_saveCoords = &DomParser::saveCSVCoords;
    saveDataToCVS("parsed/export/coords",rootItemData, f_saveCoords);
}
void DomParser::saveCoordToFile(Node *unitNode)
{
    if(unitNode == nullptr)
        unitNode = rootItemData;
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveCoords = &DomParser::saveCSVCoords;
    saveDataToCVS("parsed/export/coords" + unitNode->idName,unitNode, f_saveCoords);
}
void DomParser::saveTransform(QString sys1,QString sys2,QVector<PinNode::TYPE_INTERFACE> listInterfaces)
{
    QFile file(qApp->applicationDirPath() +  "/csv/transform/Список вставок"  + ".csv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Append);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       //out.setCodec("CP1251");
       out.setCodec("UTF-8");
       out<<sys1<<";"<<sys2<<";";
       for(auto i:listInterfaces)
           out<<QString::number(i)<<";";
       out<<"\n";
       out.flush();
    }
}
void DomParser::parseInCon(UnitNode *unit)
{
    QFile file(qApp->applicationDirPath() +  "/csv/curcuit_inside/" + unit->nameInternalFile);
    bool fileOpen = file.open(QIODevice::ReadOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream in(&file);
       in.setCodec("UTF-8");

       QString line = in.readLine();
       while(line.isEmpty() == false)
       {
           QStringList listLine = line.split(";", QString::SkipEmptyParts);

           if(listLine.empty() == true)
               return;
           if(listLine.contains("разъем",  Qt::CaseInsensitive))
           {
                line = in.readLine();
               continue;
           }

           PinNode * fPinNode1 = nullptr;
           Node * fConNode1 = findNodeByIdName(listLine[0],unit,Node::E_CONNECTOR);
           if(fConNode1 != nullptr)
           {
               fPinNode1 = static_cast<PinNode *> (findNodeByIdName(listLine[1],fConNode1,Node::E_PIN));
           }

           PinNode * fPinNode2 = nullptr;
           Node * fConNode2 = findNodeByIdName(listLine[2],unit,Node::E_CONNECTOR);
           if(fConNode2 != nullptr)
           {
               fPinNode2 = static_cast<PinNode *> (findNodeByIdName(listLine[3],fConNode2,Node::E_PIN));
           }
           if(fPinNode1 != nullptr && fPinNode2 != nullptr)
           {
               unit->pins_internal.append(QPair(fPinNode1,fPinNode2));
           }
           line = in.readLine();
       };
       file.close();

    }
}
void DomParser::loadInternalConnection(void)
{
    QList<Node *> units;
    grabberNodeByType(rootItemData,Node::E_UNIT,units);

    for(auto i:units)
    {
        UnitNode *unit = static_cast<UnitNode *> (i);
        if(unit->nameInternalFile.isEmpty() == false)
        {
            parseInCon(unit);

        }
    }
}
void DomParser::recSaveLocationBetween(Node* startNode, QTextStream& out, QString filter)
{
    if(startNode->type() == Node::E_WIRE )
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->parent->type() == Node::E_PIN)
        {
            PinNode *pin = static_cast<PinNode* > (wire->parent);
            if(pin->parent->parent->type() == Node::E_UNIT)
            {
                UnitNode *unitFrom = static_cast<UnitNode* > (pin->parent->parent);
                if(wire->fullConnected == true)
                {
                    //UnitNode *unitTo = static_cast<UnitNode* > (wire->toPin->parent->parent);
                    //if((unitFrom->idUnitLocation == filter) != (unitTo->idUnitLocation == filter))
                    if(unitFrom->idUnitLocation == filter)
                    {
                        out<<pin->parent->parent->idName<<";"
                          <<pin->parent->idName<<";"
                          <<pin->idName<<";";
                        out<<wire->idName<<";";

                        out<<wire->toPin->parent->parent->idName<<";"
                            <<wire->toPin->parent->idName<<";"
                            <<wire->toPin->idName<<";";

                        out<<wire->typeWire()<<";";
                        out<<pin->strTypeI + pin->prefTypeI<<";";
                        out<<"\n";
                    }
                }
            }
        }
    }
    for(auto i:startNode->child)
    {
        recSaveLocationBetween(i,out,filter);
    }
}
void DomParser::saveConnectionLocations(Node *rootNode,QString location1)
{
    //std::function<void(DomParser&, Node*,QTextStream&,QString)> f_saveWires = &DomParser::recSaveLocationBetween;

    QFile file(qApp->applicationDirPath() +  "/csv/parsed/export" + "wires_" + location1 + ".csv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       out.setCodec("UTF-8");
       recSaveLocationBetween(rootNode,out,location1);
    }
    //saveDataToCVS("wires_" + location1 , rootNode,f_saveWires);
}
void DomParser::fillGeometryUnit(Node* root)
{
    for(auto i:listGeo)
    {
        Node * n = findNodeByIdName(i.id,root,Node::E_UNIT);
        if(n != nullptr)
        {
            UnitNode * node     = static_cast<UnitNode* >(n);
            node->idUnitLocation   = i.parent;
            node->isStend       = i.isStend;
        }
    }
}
void DomParser::saveDataBase()
{
    //! сохранение данных в cvs
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveDataItems    = &DomParser::saveAllInfo;
    std::function<void(DomParser&, Node*,QTextStream&)> f_saveDataSystem = &DomParser::saveAllUnit;


    saveDataToCVS("parsed/base/database"    ,rootItemData,f_saveDataItems);
    saveDataToCVS("parsed/export/system"   ,rootItemData,f_saveDataSystem);
}
void DomParser::saveCSVCon(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_WIRE)
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->parent->type() == Node::E_PIN)
        {
            PinNode *pin = static_cast<PinNode* > (wire->parent);
            if(pin->io == PinNode::E_OUT)
            {
                 if(wire->fullConnected == true)
                 {
                     out<<pin->parent->parent->idName<<";"
                       <<pin->parent->idName<<";"
                       <<pin->idName<<";";
                     out<<wire->idName<<";";

                     if(wire->toPin != nullptr)
                     {
                        out<<wire->toPin->parent->parent->idName<<";"
                            <<wire->toPin->parent->idName<<";"
                            <<wire->toPin->idName<<";";

                      }else
                     {
                         out<<"-"<<";"
                             <<"-"<<";"
                             <<"-"<<";";
                     }
                     out<<wire->typeWire()<<";";
                     out<<pin->strTypeI + pin->prefTypeI<<";";
                     out<<"\n";
                 }
            }
        }
    }
    for(auto i:startNode->child)
    {
        saveCSVCon(i,out);
    }
}
QString DomParser::findLength(Node* idSys1,Node *idSys2)
{
    if(idSys1 == nullptr || idSys2 == nullptr)
        return QString("");
    for(auto i:vecLength)
    {
        Node *unit1 = findNodeByType(idSys1,Node::E_UNIT,EDirection::E_UP);
        Node *unit2 = findNodeByType(idSys2,Node::E_UNIT,EDirection::E_UP);

        if((i[0] == unit1->idName && i[1] == unit2->idName) ||
            (i[0] == unit2->idName && i[1] == unit1->idName))
        {
            return i[2];
        }
    }
    return QString("");
}
void DomParser::recSaveCSVCoords(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode* > (startNode);
        for(auto i:unit->coords)
        {
            CoordNode * coord = static_cast <CoordNode *> (i);
            for(auto j: coord->wires)
            {


                PinNode *pin = nullptr;
                //if(j->parent->type() != Node::E_PIN)
                //    continue;

                pin = static_cast<PinNode* > (j->parent);
                if(pin->strCord.isEmpty() == true)
                    continue;

                //if(pin->io == PinNode::E_OUT)
                //{
                    ConnectorNode *c = static_cast<ConnectorNode* > (pin->parent);
                    out<<pin->nameSignal<<";";
                    out<<c->parent->idName<<";";
                    out<<c->typeConnectorWire<<";";
                    out<<c->idName <<";";
                    out<<pin->idName << ";";
                    out<<j->idName<<";";
                    out<<pin->strCord<<";";

                    PinNode *toPin = static_cast<PinNode* > (j->toPin);
                    if(toPin == nullptr)
                    {
                       out<<tr("-")<<";"<<";";
                       out<<tr("свободный конец")<<";"<<";";

                    }
                    else
                    {
                        out<<toPin->idName<<";";
                        ConnectorNode *c = static_cast<ConnectorNode* > (toPin->parent);
                        out<<c->idName <<";";
                        out<<c->typeConnectorWire<<";";
                        out<<c->parent->idName<<";";
                    }
                    out<<j->typeWire()<<";";
                    out<<pin->strIDWire << ";";
                    out<<pin->strTypeWirePin <<";";
                    out<<findLength(j,j->toPin)<<";";
                    out<<"\n";

                //}
            }
        }
    }
    for(auto i:startNode->child)
    {
        recSaveCSVCoords(i,out);
    }
}

 void DomParser::saveCSVConnection(Node* rootNode, QTextStream& out)
 {
        out.setCodec("UTF-8");
        out<<tr("Блок(Источник)")<<";"<<tr("Разъем(Источник)")<<";"<<tr("Клемма(Источник)")<<";"
           <<tr("Бирка провода") <<";"
           <<tr("Блок(Приемник)")<<";"<<tr("Разъем(Приемник)")<<";"<<tr("Клемма(Приемник)")<<";"
           <<tr("Тип провода") <<";"<<tr("Интерфейс")<<"\n";
        out.flush();
        saveCSVCon(rootNode,out);
 }
 void DomParser::saveCSVCoords(Node* rootNode, QTextStream& out)
 {
        out.setCodec("UTF-8");
        out<<tr("Сигнал")<<";"<<tr("Идентификатор блока1")<<";"<<tr("Тип разъем")<<";"<<tr("Назван. разъема")<<";"<<tr("Клемма")<<";"<<tr("Бирка")<<";"
           <<tr("Жгут") <<";"
           <<tr("Клемма")<<";"<<tr("Назван. разъема")<<";"<<tr("Тип Разъем")<<";"<<tr("Идентификатор блока2")<<";"<<tr("Тип провод")<<";"
           <<tr("Идент. провода")<<";" <<tr("Тип жилы")<<";"<<tr("Длина")<<"\n";
        out.flush();
        recSaveCSVCoords(rootNode,out);
 }
bool DomParser::hasConnectThrough(WireNode* wireNode,QList<Node*> unitTransit)
{
  //  if(pin->child.isEmpty())
  //      return false;
    //WireNode *wireNode = static_cast<WireNode * > ( pin->child.first());
    if(wireNode->toPin != nullptr)
    {
        Node * fNode = findNodeByType(wireNode->toPin,Node::E_UNIT,EDirection::E_UP);
        if(fNode != nullptr)
        {
            //UnitNode *unitFNode = static_cast<UnitNode* > (fNode);

            for(auto k:unitTransit)
            {
                if(k == fNode)
                    return true;
            }

        }
    }
    return false;
}
bool DomParser::hasFullConnected(PinNode *pin)
{
    Node *node = findNodeByType(pin,Node::E_WIRE,EDirection::E_DOWN);
    if(node != nullptr)
    {
        WireNode *wire = static_cast<WireNode *>(node);
        return wire->fullConnected;
    }
    return false;
}

void DomParser::connectToUnits(Node *unitFrom_,
                                 QList<Node* > unitTransit)
{
    QList<Node* > pins;
    QList<Node* > pinSelected;

    QList<Node* > pinsUnitTransit;
    UnitNode *unitFrom = static_cast<UnitNode * > (unitFrom_);

    grabberNodeByType(unitFrom,Node::E_PIN,pins);

    //UnitNode *unitNode = static_cast<UnitNode* > (unitTransit.first());
    for(auto i : pins)
    {
        PinNode *pin = static_cast<PinNode *> (i);
        if(pin->switched ==true)
       {
          pinSelected.append(pin);
       }

    }

    // список систем через которые нужно провести сигналы
    for(auto i:unitTransit)
    {
         grabberNodeByType(i,Node::E_PIN,pinsUnitTransit);
    }
    // контакты выбранной системы
    for(auto i:pinSelected)
    {
        // текущий pin
        PinNode *pinSel = static_cast<PinNode *> (i);
        if(pinSel->child.isEmpty() == true )
            continue;

        QList<Node* > wires = pinSel->child;

        while(pinSel->child.size()!=1)
        {
            pinSel->child.removeLast();
        };
        WireNode *w0 = static_cast<WireNode * > (pinSel->child.first());
        //pinSel->child.erase(pinSel->child.begin(),pinSel->child.end());

//        for(auto k:wires)
//        {
//            //запустить рекурсию
//        }

//            // проверка, что текущий pin уже пропущен через транзитную систему
//            if(w0->fullConnected == false)
//                continue;

        // контакты транзитной системы
        for(auto j:pinsUnitTransit)
        {
            PinNode *pinTransit = static_cast <PinNode *> (j);

            if((pinSel->type_interface == pinTransit->type_interface)  &&
                 ((pinSel->io == PinNode::E_IN  && pinTransit->io == PinNode::E_OUT) ||
                  (pinSel->io == PinNode::E_OUT && pinTransit->io == PinNode::E_IN)) && pinSel->strTypeWirePin == pinTransit->strTypeWirePin)
            {
                WireNode* wTransit = nullptr;
                 if(pinTransit->child.isEmpty() == false)
                     wTransit = static_cast<WireNode* > (pinTransit->child.first());
                 if(pinTransit->child.isEmpty() || wTransit->fullConnected == false)
                 {
                     if(wTransit == nullptr)
                     {
                         wTransit = new WireNode (pinTransit->strLabel, pinTransit);
                     }

                     wTransit->toPin = pinSel;
                     wTransit->fullConnected = true;


                     // сохраняем Pin на который указывала 1ая система
                     PinNode *toPin = static_cast<PinNode *> (w0->toPin);
                     // теперь первая система указывает на транзитную систему
                     w0->toPin = pinTransit;
                     w0->fullConnected = true;
                     break;



//                     //correctWire(pinFree);
//                     correctWire(pinTransit);
//                     //correctWire(toPin);
//                     correctWire(pinSel);


                 }
            }
       // }
        }
}


 }

void DomParser::pasteUnitThrough(Node *unitFrom_,
                                 QList<Node* > unitTransit,
                                 QVector<PinNode::TYPE_INTERFACE> listInterfaces)
{
    QList<Node* > pins;
    QList<Node* > pinSelected;
    QList<Node* > pinsUnitTransit;
    UnitNode *unitFrom = static_cast<UnitNode * > (unitFrom_);

    grabberNodeByType(unitFrom,Node::E_PIN,pins);

    //UnitNode *unitNode = static_cast<UnitNode* > (unitTransit.first());
    for(auto i : pins)
    {
        PinNode *pin = static_cast<PinNode *> (i);
        for(auto j: listInterfaces)
        {
            if(pin->type_interface == j)
            {
                pinSelected.append(pin);
            }
        }
    }

    // список систем через которые нужно провести сигналы
    for(auto i:unitTransit)
    {
         grabberNodeByType(i,Node::E_PIN,pinsUnitTransit);
    }
    // контакты выбранной системы
    for(auto i:pinSelected)
    {
        // текущий pin
        PinNode *pinSel = static_cast<PinNode *> (i);
        if(pinSel->child.isEmpty() == true)
            continue;

        for(auto k:pinSel->child)
        {
            WireNode *w0 = static_cast<WireNode * > (k);
            // проверка, что текущий pin уже пропущен через транзитную систему
            if(hasConnectThrough(w0,unitTransit) == true || w0->fullConnected == false)
                continue;

        // контакты транзитной системы
        for(auto j:pinsUnitTransit)
        {
            PinNode *pinTransit = static_cast <PinNode *> (j);

            if(pinSel->type_interface == pinTransit->type_interface ||
               pinTransit->type_interface == PinNode::E_UNDEF_INTER)
            {
                 // если pin транзитной системы нас устраивает
                 QList<Node* > wires;
                 grabberNodeByType(pinTransit,Node::E_WIRE,wires);

                 UnitNode *tempUnit = static_cast<UnitNode *> (findNodeByType(pinTransit,Node::E_UNIT,EDirection::E_UP));

                 if(tempUnit->checkConnectedPins(pinTransit) == false)
                 {
                     // сохраним адресат

                     if(pinSel->child.isEmpty() == true)
                         continue;
                     pinTransit->strInterface = pinSel->strInterface;
                     pinTransit->type_interface = pinSel->type_interface;
                     pinTransit->strLabel = pinSel->strLabel;
                     WireNode *wireTransit = nullptr;
                     if(wires.isEmpty())
                         wireTransit = new WireNode (pinTransit->strLabel,pinTransit);
                     else
                         wireTransit = static_cast<WireNode *> (wires.first());

                     wireTransit->toPin = pinSel;
                     wireTransit->fullConnected = true;


                     WireNode *wirePinSel = static_cast<WireNode *> (k);
                     // сохраняем Pin на который указывала 1ая система
                     PinNode *toPin = static_cast<PinNode *> (wirePinSel->toPin);
                     // теперь первая система указывает на транзитную систему
                     wirePinSel->toPin = pinTransit;
                     wirePinSel->fullConnected = true;

                     if(pinSel->io == PinNode::E_IN)
                     {
                         pinTransit->strIO = "выдача";
                         pinTransit->io = PinNode::E_OUT;
                     }
                     else
                     {
                         pinTransit->strIO = "прием";
                         pinTransit->io = PinNode::E_IN;
                     }



                     // ищем свободный пин для связи транзитной системы с 2ой системой
                     PinNode* pinFree = tempUnit->findSameConnection(pinTransit);
                     WireNode *sys2 = nullptr;

                     if(toPin->child.isEmpty())
                         sys2 = new WireNode(toPin->strLabel,toPin);
                     else
                     {
                         for(auto l:toPin->child)
                         {
                             sys2 = static_cast<WireNode *> (l);
                             if(sys2->toPin == i)
                                 break;
                         }

                     }
                     sys2->toPin = pinFree;
                     if(toPin->io == PinNode::E_IN)
                     {
                          pinFree->strIO = "выдача";
                         pinFree->io = PinNode::E_OUT;
                     }
                     else
                     {
                         pinFree->strIO = "прием";
                         pinFree->io = PinNode::E_IN;
                     }

                     pinFree->strInterface = toPin->strInterface;
                     pinFree->type_interface = toPin->type_interface;

                     sys2->fullConnected = true;

                     WireNode *freeWire = static_cast<WireNode *> (pinFree->child.first());
                     freeWire->toPin = toPin;
                     freeWire->fullConnected = true;
                     correctWire(pinFree);
                     correctWire(pinTransit);
                     correctWire(toPin);
                     correctWire(pinSel);

                     break;
                 }
            }
        }
        }
}

    correctWire     (rootItemData);
    clearCoords(rootItemData);
    correctCoords(rootItemData);
 }
Node* DomParser::tracePinToFindFreePin(Node* pin,
                                       Node* prevPin,
                                       Node * fPin,
                                       QVector<QPair<PinNode *, PinNode * > > *table)
{
    Node* n = findNodeByType(pin,Node::E_UNIT,EDirection::E_UP);

    QVector<QPair<PinNode *, PinNode * > > temp_pins;

    if(n == nullptr)
        return nullptr;
    //! блок к которому принадлежит контакт
    UnitNode *unitNode = static_cast<UnitNode *>(n);
    //! текущий контакт
    PinNode  *pinTrace = static_cast<PinNode* > (pin);
    if(pin->child.isEmpty() == false)
    {
        for(auto w : pin->child)
        {
            WireNode *wire   = static_cast<WireNode * > (w);
            if((wire->toPin != nullptr || wire->fullConnected == true) && wire->toPin!=prevPin)
            {
                Node*n = tracePinToFindFreePin(wire->toPin,pin,wire->toPin);
                if(n!= nullptr)
                    return n;
            }
        }
    }
    //! промежуточный массив
    //! таблица соединений контактов внутри блока
    if(table == nullptr)
    {
        temp_pins = unitNode->pins_internal;
        table = &temp_pins;
    }
    int i = 0;

        while(!table->isEmpty() && i<table->size())
        {
            PinNode *pin1 = (*table)[i].first;
            PinNode *pin2 = (*table)[i].second;

            if(pinTrace == pin1)
            {
                pinTrace = pin2;
                table->remove(i);
                i = -1;
                Node* n= tracePinToFindFreePin(pinTrace,pin1,fPin, table);
                if(n!=nullptr)
                    return n;
            }
            else if(pinTrace == pin2)
            {
                pinTrace = pin1;
                table->remove(i);
                i = -1;
                Node* n= tracePinToFindFreePin(pinTrace,pin2,fPin,table);
                if(n!=nullptr)
                    return n;
            }
            i++;
        }


//    for(int i = 0; i<unitNode->pins_internal.size();i++)
//    {
//        PinNode *pin1 = unitNode->pins_internal[i].first;
//        PinNode *pin2 = unitNode->pins_internal[i].second;

//        if(pinTrace == pin1)
//        {
//            pinTrace = pin2;
//            Node* n= tracePinToFindFreePin(pinTrace,pin1,fPin);
//            if(n!=nullptr)
//                return n;
//        }
//        else if(pinTrace == pin2)
//        {
//            pinTrace = pin1;
//            //i = -1;
//            Node* n= tracePinToFindFreePin(pinTrace,pin2,fPin);
//            if(n!=nullptr)
//                return n;
//        }
//    }
        //Node* n= tracePinToFindFreePin(pinTrace,pin1,fPin);
        //           if(n!=nullptr)
        //               return n;

//    for(auto i : unitNode->pins_internal)
//    {
//        PinNode *pin1 = i.first;
//        PinNode *pin2 = i.second;

//        if(pinTrace == pin1)
//        {
//            pinTrace = pin2;
//            Node* n= tracePinToFindFreePin(pinTrace,pin1,fPin);
//            if(n!=nullptr)
//                return n;
//        }
//    }
    return fPin;
}
bool DomParser::hasPinInListWires(Node *pin,QList<Node *> &wireNode)
{
    for(auto i:wireNode)
    {
        WireNode *w_i   = static_cast<WireNode * > (i);
        Node* fNode = findNodeByType(w_i->toPin,Node::E_UNIT,EDirection::E_UP);
        if(fNode == pin)
        {
            for(auto w:pin->child)
            {
                 WireNode *wire   = static_cast<WireNode * > (w);
                if(wire->toPin == fNode)
                    return true;
            }
        }
    }
    return false;
}
void DomParser::traceWiresFromPin(Node *pin, Node* sampleUnit, QList<Node *> &wireNode, QList<Node *> *prohibSys)
{
    for(auto w : pin->child)
    {

        WireNode *wire   = static_cast<WireNode * > (w);
        if(wire->toPin == nullptr)
            continue;
        //! здесь должна быть функция поиска и перехода между системами
        Node* fNode = findNodeByType(wire->toPin,Node::E_UNIT,EDirection::E_UP);
        if(fNode == sampleUnit )
            wireNode.append(wire);
        else
        {
            //иначе двигаемя дальше
            UnitNode *unit = static_cast<UnitNode *> (fNode);
            PinNode *pin   = static_cast<PinNode *>  (wire->toPin);
            QList<PinNode *> pins = unit->findAllInternalConnection(pin);
            for(auto p: pins)
            {
                if(hasPinInListWires(p,wireNode) == false && prohibSys->indexOf(fNode) == -1)
                    traceWiresFromPin(p,sampleUnit,wireNode,prohibSys);
            }
        }
    }
}
bool DomParser::checkInOutPins(PinNode *pin1,PinNode *pin2)
{
    if((pin1->io == PinNode::TYPE_IO::E_IN || pin1->io == PinNode::TYPE_IO::E_OUT) &&
       (pin2->io == PinNode::TYPE_IO::E_IN  || pin2->io == PinNode::TYPE_IO::E_OUT))
    {
        if(pin1->io != pin2->io)
            return true;
    }
    return false;
}
void DomParser::updateCoords()
{
    //correctInterface(rootItemData);
    correctWire     (rootItemData);
    renameCoords(rootItemData);
    clearCoords(rootItemData);
    correctCoords(rootItemData);


}
void DomParser::pasteUnitBetween(Node *unitFrom_,
                                 QList<Node* > unitsTransit_,
                                 QList<Node* > listUnitTo_,
                                 QVector<PinNode::TYPE_INTERFACE> listInterfaces)
{
    if(unitFrom_ == nullptr || unitsTransit_.isEmpty())
        return;
    // проверяем, что не пропускаем провода через сам же блок
    for(auto l:listUnitTo_)
    {
        Node *unitTo_ = l;
    if(unitFrom_ == unitTo_)
        continue;

    QList<Node* > pinsUnitSys1;
    QList<Node* > pinsUnitTransitSel;
    QList<Node* > pinsUnitTransit;
    // список(траса) всех проводов соединяющий Sys1 и Sys2
    QList<Node* > wireUnitSys1;
    // провода полученные из wireUnitSys1 выходящие только из Sys1
    QList<Node* > wireUnitSys1Sel;
    //
    UnitNode *unitSys1 = static_cast<UnitNode * > (unitFrom_);
    UnitNode *unitSys2   = static_cast<UnitNode * > (unitTo_);

    // собираем все пины в один список от Системы 1
    grabberNodeByType(unitSys1,Node::E_PIN,pinsUnitSys1);
    // убираем все пины которые не связанны с Системой 2
    for(auto i: pinsUnitSys1)
    {
        PinNode *pin_out = static_cast<PinNode *> (i);

        if(i->child.isEmpty() == true || pin_out->io != PinNode::E_OUT)
            continue;

        traceWiresFromPin(i, unitSys2,wireUnitSys1,&unitsTransit_);
    }
//    for(auto k: )
//    for(auto i: pinsUnitSys1)
//    {
//        PinNode *pin_out = static_cast<PinNode *> (i);

//        if(i->child.isEmpty() == true || pin_out->io != PinNode::E_OUT)
//            continue;

//        traceWiresFromPin(i, unitSys2,wireUnitSys1);
//    }
    // проверяем, что не проводили провода через транзитную систему
    // оставляем pin от 1ой системы
    //pinsUnitSys1.clear();
    for(auto i: wireUnitSys1)
    {
        // здесь должна быть функция поиска и перехода между системами
        Node* fNode = findNodeByType(i->parent,Node::E_UNIT,EDirection::E_UP);
        if(fNode == unitSys1)
            wireUnitSys1Sel.append(i);
    }
    // выбираем все контакты из транзитных систем
    for(auto i: unitsTransit_)
    {
        grabberNodeByType(i,Node::E_PIN,pinsUnitTransit);
    }
    // оставялем только свободные
    for(auto i:pinsUnitTransit)
    {
        if(i->child.isEmpty())
        {
            pinsUnitTransitSel.append(i);
            continue;
        }
        for(auto j : i->child)
        {
            WireNode *wire = static_cast<WireNode *> (j);
            if(wire->fullConnected == false)
                pinsUnitTransitSel.append(i);
        }

    }
    if(pinsUnitTransitSel.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("недостаточно свободных клемм");
        msgBox.exec();
    }
    // находим соответствующие контакты
    for(auto i : wireUnitSys1Sel)
    {
        for(auto j: pinsUnitTransitSel)
        {
            PinNode *parentPin = static_cast<PinNode *> (i->parent);
            PinNode *transitPin = static_cast<PinNode *> (j);
            if(checkInOutPins(parentPin,transitPin) &&
               (parentPin->type_interface == transitPin->type_interface) &&
                    /*transitPin->child.isEmpty() == true &&*/ parentPin->type_interface!=PinNode::E_SHEILD_I)
            {
                Node *endP = tracePinToFindFreePin(transitPin);
                if(endP == nullptr)
                    break;

                UnitNode *pUnit   = static_cast<UnitNode * > (findNodeByType(endP,Node::E_UNIT,EDirection::E_UP));
                PinNode *pNode = static_cast<PinNode* > (endP);
                PinNode* fp = pUnit->findSameConnection(pNode,parentPin->io);
                if(fp != nullptr)
                {
                    //PinNode* fp = static_cast<PinNode *> (p1);
//                    if(parentPin->io == fp->io)
//                    {
                        WireNode *wireSys1Sel = static_cast<WireNode *> (i);
                        Node* saveP = wireSys1Sel->toPin;
                        wireSys1Sel->toPin = transitPin;
                        wireSys1Sel->fullConnected = true;

                        if(transitPin->child.isEmpty())
                        {
                            WireNode *w = new WireNode(transitPin);
                            w->toPin = parentPin;
                            w->fullConnected = true;
                    }else {
                                WireNode *w = static_cast<WireNode* > (transitPin->child[0]);
                                w->toPin = parentPin;
                                w->fullConnected = true;

                    }

                        if(fp->child.isEmpty())
                        {
                            WireNode *w0 = new WireNode(fp);
                            w0->toPin = saveP;
                            w0->fullConnected=true;
                            // найдем провод который связан с исходным контактом
                            for(auto k:saveP->child)
                            {
                                WireNode *wk = static_cast<WireNode *> (k);
                                if(wk->toPin == parentPin)
                                {
                                    wk->toPin = fp;
                                    wk->fullConnected = true;
                                }
                            }

                        }else
                        {
                           WireNode* wfp= static_cast<WireNode* >(fp->child[0]);
                           wfp->toPin = saveP;
                           wfp->fullConnected = true;
                           for(int k =0 ;k<saveP->child.size();k++)
                           {
                               WireNode * w1 = static_cast<WireNode*> (saveP->child[k]);
                               if(w1->toPin == parentPin)
                               {
                                   w1->toPin = fp;
                                   w1->fullConnected = true;
                                   break;
                               }
                           }

                        }
                        break;




                }


            }
        }
        correctWire     (rootItemData);
        clearCoords(rootItemData);
        correctCoords(rootItemData);
    }
}

    // п
//    grabberNodeByType(unitFrom,Node::E_WIRE,nodeWireConnect);
//    grabberNodeByType(unitTransit,Node::E_WIRE,nodeWireTransit);

//    for(auto i:nodeWireConnect)
//    {
//        WireNode *wire = static_cast<WireNode* > (i);
//        if(wire->fullConnected == true)
//        {
//            for(auto j:nodeWireTransit)
//            {
//                WireNode *wireTr = static_cast<WireNode* > (j);
//                if(wireTr->fullConnected == false)
//                {
//                    wire->toPin = wireTr->parent;
//                    wireTr->toPin = wire->parent;
//                    wireTr->fullConnected = true;
//                }
//            }
//        }
//    }
}
//void DomParser::swapNode()
//{

//}
void DomParser::mergeNodes(Node* root,Node* from)
{
    bool isFind = false;
    bool isFindCurcuit = false;
    if(root->child.isEmpty() == true)
    {
        Node* add_node = from->clone();
        root->addChild(add_node);

    }else
    {
        for(auto i:root->child)
        {
            if(i->idName == from->idName)
            {
                if(i->type() == Node::E_CONNECTOR && from->type() == Node::E_CONNECTOR )
                {
                    ConnectorNode * con = static_cast<ConnectorNode* > (i);
                    ConnectorNode * conFrom = static_cast<ConnectorNode* > (from);

                    mergeString(con->typeConnectorBlock,conFrom->typeConnectorBlock);
                    mergeString(con->typeConnectorWire,conFrom->typeConnectorWire);

                }
                if(i->type() == Node::E_PIN  && from->type() == Node::E_PIN )
                {
                    PinNode * pin       = static_cast<PinNode* > (i);
                    PinNode * pinFrom   = static_cast<PinNode* > (from);
                    if(pin->strNumClone.toInt()<pinFrom->strNumClone.toInt())
                    {
                        pin->strNumClone = pinFrom->strNumClone;
                    }
                    if(pinFrom->strCircuit.isEmpty() == false )
                    {
                        for(auto j: pinFrom->strCircuit)
                        {
                            isFindCurcuit = false;
                            for(auto k: pin->strCircuit)
                            {
                                if(j == k)
                                {
                                    isFindCurcuit = true;
                                    break;
                                }
                            }
                            if(isFindCurcuit == false)
                                pin->strCircuit.append(j);
                        }
                    }
                    mergeString(pin->strCord,pinFrom->strCord);
                    mergeString(pin->strTypeWirePin,pinFrom->strTypeWirePin);
                    if(pin->strCord.isEmpty() && pinFrom->strCord.isEmpty())
                    {
                        UnitNode *funit = static_cast<UnitNode* > (findNodeByType(pin,Node::E_UNIT,EDirection::E_UP));
                        UnitNode *funitFrom = static_cast<UnitNode* > (findNodeByType(pin,Node::E_UNIT,EDirection::E_UP));

                        if(funit!= nullptr && funitFrom!=nullptr)
                        {

                            pin->strCord = pinFrom->strCord = (mergeString(funit->nameCoord,funitFrom->nameCoord) + " " + funit->idName + pin->parent->idName );
                        }

                    }

                    //! добавление размноженных проводов
                    Node *findW = nullptr;
                    QList<Node* > tempNode = pin->child;
                    for(auto w0:pinFrom->child)
                    {
                        findW = nullptr;

                        int k = 0;
                        while(!tempNode.isEmpty() && k<tempNode.size())
                        {
                            Node *w1 = tempNode[k];
                            if(w0->idName == w1->idName)
                            {
                                findW = w1;
                                tempNode.removeAt(k);
                                break;
                            }
                            k++;
                        }
                        if(findW == nullptr)
                        {
                            findW = w0->clone();
                            pin->addChild(findW);
                            //findW->addParent(pin);
                            //pin->child.append(findW);
                        }
                    }
                    if(pin->child.size() != pinFrom->child.size())
                    {
                        outLog<<"wires not compare p1"<<pin->pathName<<" p2 = " << pinFrom->pathName<<"\n";
                    }
                }
                if(i->type() == Node::E_UNIT && from->type() == Node::E_UNIT)
                {
                    UnitNode *unit       = static_cast<UnitNode* > (i);
                    UnitNode *unitFrom   = static_cast<UnitNode* > (from);

                    mergeString(unit->alias,unitFrom->alias);
                    /*
                    if(unit->alias != unitFrom->alias)
                    {
                        if(unit->alias.isEmpty() && unitFrom->alias.isEmpty() == false)
                        {
                            unit->alias = unitFrom->alias;
                        }else if(unitFrom->alias.isEmpty() == true && unit->alias.isEmpty() == false)
                        {
                            unitFrom->alias = unit->alias;
                        }
                    }*/
                }

                //! должны проверить
                isFind = true;
                for(auto j:from->child)
                    mergeNodes(i,j);
            }
        }
        if(isFind == false)
        {
            Node* add_node = from->clone();
            root->addChild(add_node);
        }
    }
}

Node* DomParser::findNodeByIdName(QString nameItem, Node *parent, Node::Type t)
{
    return recFindNodeWithIdName(nameItem, parent, t);
}
Node* DomParser::recFindNodeWithIdName(QString &idName,Node *startNode, Node::Type t)
{
    Node *lStartNode = startNode;

    if(startNode == nullptr)
    {
        if(dataNodes == nullptr)
            return nullptr;
        startNode   = dataNodes;
        lStartNode  = dataNodes;
    }

    if(idName == startNode->idName && (t == Node::E_EMPTY || startNode->type() == t))
        return startNode;

    for(auto &node:lStartNode->child)
    {
        if(node->idName == idName && (t == Node::E_EMPTY || node->type() == t))
        {
            return static_cast<Node* >  (node);
        }
    }
    for(auto &node:lStartNode->child)
    {
        Node* tempNode=recFindNodeWithIdName(idName,static_cast<Node* > (node),t);
        if(tempNode != nullptr)
            return tempNode;
    }
    return nullptr;
}
void DomParser::parseTransData(QString line, Node *parent)
{
    QStringList listLine = line.split(";", QString::SkipEmptyParts);

//    if(listLine.empty() == true)
//        return;

//    if(listLine.size() != 2)
//        return;
    vecTransform.append(listLine);
}
void DomParser::parseAlias(QString line, Node *parent)
{
    QStringList listLine = line.split(";", QString::SkipEmptyParts);

    if(listLine.empty() == true)
        return;

    if(listLine.size() != 2)
        return;
    vecAlias.append(QPair(listLine[0],listLine[1]));
}
void DomParser::parseLength(QString line, Node *parent)
{
    QStringList listLine = line.split(";", QString::SkipEmptyParts);

//    if(listLine.empty() == true)
//        return;

//    if(listLine.size() != 2)
//        return;
    vecLength.append(listLine);
}
void DomParser::parseData(QString line, Node *parent)
{
    QStringList listLine = line.split(";", QString::SkipEmptyParts);

    if(listLine.empty() == true)
        return;

    if(listLine.size() != E_CORD +1)
        return;

    if((listLine[0] == "Сигнала" || listLine[0] == "Сигнал") &&
       (listLine[1] == "Блок" || listLine[1] == "Блоки")  )
        return;

    Node *node = nullptr;
    Node *nodeParent = parent;
//    node = findNodeByIdName(listLine[E_ID_SYSTEM], nodeParent,Node::E_SYSTEM);

//    if(node == nullptr)
//        nodeParent = new SystemNode(listLine[E_ID_SYSTEM],nodeParent);
//    else
//        nodeParent = node;

    node = findNodeByIdName(listLine[E_ID_UNIT], nodeParent,Node::E_UNIT);
    if(node == nullptr)
    {
        nodeParent = new UnitNode(listLine[E_ID_UNIT], nodeParent);
        nodeParent->displayName = listLine[E_UNIT_NAME];
    }else
        nodeParent = node;

    node = findNodeByIdName(listLine[E_CONNECTOR], nodeParent, Node::E_CONNECTOR);
    if(node == nullptr)
        nodeParent = new ConnectorNode(listLine[E_CONNECTOR],
                                       listLine[E_TYPE_CONNECTOR_BLOCK],
                                       listLine[E_TYPE_CONNECTOR_WIRE],
                                       nodeParent);
    else
        nodeParent = node;
    //! создание объекта - клемма в разъеме
    node = findNodeByIdName(listLine[E_PIN], nodeParent, Node::E_PIN);
    if(node == nullptr)
        nodeParent = new PinNode(listLine[E_PIN],
                                 listLine[E_NAME],
                                 listLine[E_IO],
                                 listLine[E_MULT],
                                 listLine[E_SW],
                                 listLine[E_LABEL],
                                 listLine[E_TYPE_WIRE],
                                 listLine[E_TYPE_I],
                                 listLine[E_SET_TYPE_I],
                                 listLine[E_CURCUIT],
                                 listLine[E_CORD],
                                 listLine[E_TYPE_WIRE_PIN],
                                 listLine[E_ID_WIRE],
                                 nodeParent);
    else
        nodeParent = node;
}
void DomParser::parseLocation(QString line, Node *parent)
{
    Node *node = nullptr;
    Node *nodeParent = parent;

    QStringList listLine = line.split(";", QString::SkipEmptyParts);

    if(listLine.empty() == true || listLine.size() != (E_GEO_FACTORY_ID +1))
        return;
    //! пропустить первую строчку
    if(listLine[0].contains("название", Qt::CaseInsensitive))
        return;

    node = findNodeByIdName(listLine[E_GEO_ID], nodeParent,Node::E_UNIT);
    if(node == nullptr)
    {
        node = new UnitNode(listLine[E_GEO_FULL],
                            listLine[E_GEO_SHORT],
                            listLine[E_GEO_STEND],
                            listLine[E_GEO_INSTALL],
                            listLine[E_GEO_TRANSIT],
                            listLine[E_GEO_ID],
                            listLine[E_GEO_LOCATION],
                            listLine[E_GEO_PARENT_SYS],
                            listLine[E_GEO_SIZE],
                            listLine[E_GEO_POS],
                            listLine[E_GEO_CLASS],
                            listLine[E_GEO_ALIAS],
                            listLine[E_GEO_NAME_COORD],
                            listLine[E_GEO_FACTORY_ID],
                            nodeParent);
    }

//    TGeometry geo;
//    geo.fullName  = listLine[E_GEO_FULL  ];
//    geo.shortName = listLine[E_GEO_SHORT ];
//    geo.isStend   = listLine[E_GEO_STEND ].toLower() == "да" ? true:false;
//    geo.id        = listLine[E_GEO_ID    ];
//    geo.parent    = listLine[E_GEO_PARENT];
//    geo.size      = listLine[E_GEO_SIZE  ];
//    geo.coord     = listLine[E_GEO_COORD ];
//    listGeo.push_back(geo);
}


QString DomParser::mergeString(QString &value1,QString &value2)
{
    if(value1 != value2)
    {
        if(value1.isEmpty() && value2.isEmpty()==false)
        {
            value1 = value2;
            return value1;
        }else if(value1.isEmpty() == false && value2.isEmpty())
        {
            value2 = value1;
            return value2;
        }
    }
    return QString("");
}
void DomParser::recFindWireWithout(Node *wire, Node *startNode)
{
    if(startNode->parent!= nullptr)
    {
        if(startNode->type() == Node::E_WIRE &&
           startNode->parent->id != wire->parent->id &&
           wire->idName == startNode->idName )
        {
            WireNode *w  = static_cast<WireNode* > (wire);
            WireNode *w0 = static_cast<WireNode* > (startNode);

            if(w->toPin == nullptr && w0->fullConnected == false)
            {
                w->toPin = startNode->parent;
                w0->toPin = w->parent;
                if(w0->toPin != nullptr)
                {
                    PinNode *pin0 = static_cast<PinNode* >(w0->toPin);
                    PinNode *pin  = static_cast<PinNode* >(w->toPin);
                    if(pin->io != pin0->io)
                    {
                        w0->fullConnected = true;
                        w->fullConnected  = true;
                        Node *u0 = findNodeByType(pin0,Node::E_UNIT,EDirection::E_UP);
                        Node *u  = findNodeByType(pin,Node::E_UNIT,EDirection::E_UP);
                        if(pin->nameSignal.contains(u0->shortName)==false)
                            pin->nameSignal+=" (Из " + u0->shortName + ")";
                        if(pin0->nameSignal.contains(u->shortName)==false)
                            pin0->nameSignal+=" (Из " + u->shortName + ")";

                        mergeString(pin->strCord,pin0->strCord);
                        mergeString(pin->strTypeWire,pin0->strTypeWire);
                        mergeString(pin->strTypeI,pin0->strTypeI);

                        mergeString(pin->strIDWire,pin0->strIDWire);
                        mergeString(pin->strTypeWirePin,pin0->strTypeWirePin);

                    }
                }
            }
        }
    }
    for(auto i:startNode->child)
    {
        recFindWireWithout(wire,i);
    }
}
void DomParser::recFindWire(Node *startNode, Node *rootNode)
{
    if(startNode->type() == Node::E_WIRE)
    {
        recFindWireWithout(startNode, rootNode);
    }else
    {
        for(auto i:startNode->child)
        {
            recFindWire(i, rootNode);
        }
    }
}
bool DomParser::connectWires(Node *rootNode)
{
    recFindWire(rootNode,rootNode);
    return true;
}
bool DomParser::openFileDesData(const QString &nameDir,QList<Node* > &listNode,std::function<void(DomParser&, QString,Node*)> parse)
{
    bool ok = true;

    QStringList filtres;
    filtres<<"*.csv";
    QDir dir(nameDir);    
    QFileInfoList fileList = dir.entryInfoList(filtres, QDir::Files);
    for(auto &f:fileList)
    {
        QFile file(f.absoluteFilePath());
        bool openFile =  file.open(QIODevice::ReadOnly | QIODevice::Text);
        //! создаем корень структуры
        listNode.append(new Node(f.baseName()));
        if(openFile == true)
        {
            QString errMsg  = "";
            QTextStream in(&file);
            in.setCodec("UTF-8");
            //in.setCodec("CP1251");
            QString line = in.readLine();
            while(line.isEmpty() == false)
            {
                parse(*this,line,listNode.back());
                line = in.readLine();
            };
            file.close();
        }
    }
    return ok;
}

void DomParser::saveForGraphviz(QString namePath, QString nameFile, Node* rootNode, std::function<void(DomParser&, Node *, QTextStream&)> funcSave)
{
    QFile file(qApp->applicationDirPath() +  namePath + nameFile + ".gv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       out.setCodec("UTF-8");
       out<<"digraph pvn {ratio = \"expand\" ; rankdir = \"LR\" ;  \n graph [ ranksep = 20 ]    node [     fontsize = \"16\"           shape = \"ellipse\"      ];  edge [   ];  \n";
       funcSave(*this,rootNode,out);

       out<<"\n}";
       out.flush();
    }
    QString program = "dot";
    //program = "./tools/7z.exe";
    QStringList arguments; arguments.clear(); arguments<< "-Tpdf" << qApp->applicationDirPath() +  "/csv/parsed/curcuit/gv/" + nameFile + ".gv"
                                            << "-o" << qApp->applicationDirPath() +  "/csv/parsed/curcuit/pdf/" + nameFile + ".pdf";
    //Й = new QProcess(this);
    procDot.start(program,arguments);
    procDot.waitForFinished();
}
bool DomParser::checkConnectionUnit(Node* sampleNode, Node *node)
{
    //if(sampleNode->type() != Node::E_UNIT)
    Node *fNode = findNodeByType(node, Node::E_UNIT,EDirection::E_UP);
    if(fNode->idName == sampleNode->idName)
        return true;

    return false;
}
void DomParser::grabberNodeByType(Node *root,Node::Type t,QList<Node* > &list)
{
    if(root->type() == t)
    {
        list.push_back(root);
    }
    for(auto i:root->child)
    {
        grabberNodeByType(i,t,list);
    }
}
QList<Node* > DomParser::findConnectionPins(Node* unit1, Node* checkForUnit)
{
    QList<Node* > list;
    if(checkForUnit->type() != Node::E_UNIT || unit1->type() != Node::E_UNIT)
        return list;
    QList<Node* > listPin;
    grabberNodeByType(checkForUnit,Node::E_PIN,listPin);

    //Node *wNode = findNodeByType(checkForUnit,Node::E_WIRE,EDirection::E_DOWN);

    for(auto i:listPin)
    {
        QList<Node* > listWire;
        grabberNodeByType(i,Node::E_WIRE,listWire);
        if(listWire.isEmpty() == false)
        {
            for(auto j:listWire)
            {
                WireNode *wire = static_cast<WireNode *>(j);
                if(wire->toPin != nullptr)
                {
                    if(checkConnectionUnit(unit1, wire->toPin) == true) //связан ли unit1 c проводом указанным вторым аргументом
                    {
                        Node *tNode = findNodeByType(wire,Node::E_PIN,EDirection::E_UP);
                        if(list.indexOf(tNode) == -1)
                            list.push_back(tNode);
                    }
                }
            }
        }
    }

    return list;
}
QList<Node* > DomParser::selectConnector( QList<Node* > listPin)
{
    QList<Node* > list;
    for(auto i:listPin)
    {
        int res = list.indexOf(i->parent);
        if(res == -1)
            list.push_back(i->parent);
    }
    return list;
}
void DomParser::saveInterface(Node *startNode, QTextStream& out)
{

}

void DomParser::saveWires(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_WIRE)
       {
           WireNode *wire = static_cast<WireNode* > (startNode);
           if(wire->parent->type() == Node::E_PIN)
           {
               PinNode *pin = static_cast<PinNode* > (wire->parent);
               if(pin->io == PinNode::E_IN)
               {
                       out<<"\""<<wire->idName<<"\"";
                       out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
                            QString::number(pin->parent->child.indexOf(pin))+
                            "[label=" + "\"" + wire->idName + "\"";
                       if(wire->fullConnected == false)
                       {
                           //outLog<<"Провод неполностью подключен"<<wire->pathName<<"\n";
                           out<<" fillcolor=\"red\" ";
                       }
                       else if(pin->switched == true)
                           out<<" fillcolor=\"blue\" ";

                       if(pin->type_interface == PinNode::E_RK)
                           out<<" style=\"dashed\"";
                       else if(pin->type_interface == PinNode::E_ARINC_429)
                           out<<" style=\"dotted\"";
                       else if(pin->type_interface == PinNode::E_POWER)
                           out<<" style=\"bold\"";
                       else
                           out<<" style=\"filled\"";


                           out<<"];\n";
//                           if(wire->toPin != nullptr)
//                           {
//                               saveGraphConnector(wire->toPin->parent,out);
//                               saveGraphWire(wire->toPin,out);
//                           }
               }else
               {

                   out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(pin->parent->child.indexOf(pin));
                   //if(wire->toPin == nullptr)
                        out<<"->\""<<wire->idName + "\"" +"[label=" + "\"" +wire->idName + "\"";
                        if(wire->fullConnected == false)
                        {

                            out<<" fillcolor=\"red\" ";
                        }
                        else if(pin->switched == true)
                             out<<" fillcolor=\"blue\" ";


                        if(pin->type_interface == PinNode::E_RK)
                            out<<" style=\"dashed\"";
                        else if(pin->type_interface == PinNode::E_ARINC_429)
                            out<<" style=\"dotted\"";
                        else if(pin->type_interface == PinNode::E_POWER)
                            out<<" style=\"bold\"";
                        else
                            out<<" style=\"filled\"";


                            out<<"];\n";

//                     if(wire->toPin != nullptr)
//                     {
//                         saveGraphConnector(wire->toPin->parent,out);
//                         saveGraphWire(wire->toPin,out);
//                     }
               }

           }
       }
    for(auto i:startNode->child)
    {
       saveWires(i,out);
    }
}
void DomParser::saveSubGraph(Node *unit,Node* con,QList<Node* > listPin, QTextStream& out)
{
    out<<"subgraph cluster" +  QString::number(unit->id) + " \n{ fontcolor=\"blue\" fillcolor=\"grey\" style=\"filled\" label=\""+
         unit->displayName + "["+unit->idName+"]"+ "\"\n";

    ConnectorNode *coneLabel = static_cast<ConnectorNode* > (con);
    out<<"subgraph cluster" +  QString::number(con->id) + " \n{ fontcolor=\"black\" label=\""+con->idName+ "[" + coneLabel->typeConnectorWire + "]"+"\"\n";

    int k =0;
    out<<"\"node"+ QString::number(con->id) + "\"[ label = \"";
    for(auto j:listPin )
    {
        if(j->parent == con)
        {
            if(k>0)
                out<<"|";
            //out<<"<f"+QString::number(k) + "> " + j->idName;
            int index = j->parent->child.indexOf(j);
            out<<"<f"+QString::number(index) + "> [" + j->idName +"] : " + ((PinNode*)j)->nameSignal;

            k++;
        }
    }
    out<<"\" \n shape = \"record\"];\n";
    out<<"}\n";
    out<<"}\n";
}
void DomParser::saveNeighborsToGV(Node* rootNode, QList<Node* > listUnit, QTextStream& out)
{
    for(auto i: listUnit)
    {
        if(i == rootNode)
            continue;

        //! поиск всех пинов связывающих с данным блоком
        QList<Node* > listPin = findConnectionPins(rootNode, i);

        QList<Node* > listCon  = selectConnector(listPin);

        for(auto x:listCon)
        {
            saveSubGraph(i,x,listPin,out);
        }
        for(auto x:listPin)
            saveWires(x,out);
    }
//    //! поиск всех пинов связывающих с данным блоком
    QList<Node* > listPin ;
    grabberNodeByType(rootNode,Node::E_PIN, listPin);
    QList<Node* > listCon  = selectConnector(listPin);

    for(auto x:listCon)
    {
        saveSubGraph(rootNode,x,listPin,out);
    }
    for(auto x:listPin)
        saveWires(x,out); 
}
void DomParser::saveNeighborsToGV(Node* rootNode, Node* rootNode2, QTextStream& out)
{
    //! поиск всех пинов связывающих с данным блоком
    QList<Node* > listPin = findConnectionPins(rootNode, rootNode2);
    QList<Node* > listCon  = selectConnector(listPin);

    for(auto x:listCon)
        saveSubGraph(rootNode2,x,listPin,out);
    for(auto x:listPin)
        saveWires(x,out);

    //! поиск всех пинов связывающих с данным блоком
    QList<Node* > listPin2 ;
    grabberNodeByType(rootNode,Node::E_PIN, listPin2);
    QList<Node* > listCon2  = selectConnector(listPin2);

    for(auto x:listCon2)
        saveSubGraph(rootNode,x,listPin2,out);

    for(auto x:listPin2)
        saveWires(x,out);
}
void DomParser::saveCoordsToGV(Node* rootNode, Node* rootNode2, QTextStream& out)
{
    //! поиск всех пинов связывающих с данным блоком
    QList<Node* > listPin = findConnectionPins(rootNode, rootNode2);
    QList<Node* > listCon  = selectConnector(listPin);

    for(auto x:listCon)
        saveSubGraph(rootNode2,x,listPin,out);
    for(auto x:listPin)
        saveWires(x,out);

    //! поиск всех пинов связывающих с данным блоком
    QList<Node* > listPin2 ;
    grabberNodeByType(rootNode,Node::E_PIN, listPin2);
    QList<Node* > listCon2  = selectConnector(listPin2);

    for(auto x:listCon2)
        saveSubGraph(rootNode,x,listPin2,out);

    for(auto x:listPin2)
        saveWires(x,out);
}
//! Показать список соединений между двумя группой блоков
void DomParser::showConnectionBetween(QString group1, QString group2)
{
    //! фильтр не задан
    if(group1.isEmpty() && group2.isEmpty())
        return;

    if(group2.isEmpty() == true)
    {
        //! рекурсия с загрузкой данных
    }
}

void DomParser::saveForGraphvizForNode(QString nameFile, Node* rootNode)
{
    QFile file(qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       out.setCodec("UTF-8");
       out<<"digraph pvn {ratio = \"expand\" ; rankdir = \"LR\" ;  \n graph [  ranksep = 20 ]    node [     fontsize = \"16\"           shape = \"ellipse\"      ];  edge [   ];  \n";

       QList<Node* > listUnit;
       // собрать всех соседей
       findNeighborUnit(rootNode,listUnit);

       saveNeighborsToGV(rootNode, listUnit, out);
       out<<"\n}";
       out.flush();
    }
    QString program = "dot";
    //program = "./tools/7z.exe";
    QStringList arguments; arguments.clear(); arguments<< "-Tpdf" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv"
                                            << "-o" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".pdf";
    //Й = new QProcess(this);
    procDot.start(program,arguments);
    procDot.waitForFinished();
}
void DomParser::saveForGraphvizForCoords(QString nameFile, Node* rootNode)
{
    QFile file(qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       out.setCodec("UTF-8");
       out<<"digraph pvn {ratio = \"expand\" ; rankdir = \"LR\" ;  \n graph [  ranksep = 20 ]    node [     fontsize = \"16\"           shape = \"ellipse\"      ];  edge [   ];  \n";

       QList<CoordNode* > listUnit;
       // собрать всех соседей
       //findNeighborUnit(rootNode,listUnit);
       findAllCoordsUnit(rootNode,listUnit);
       //saveCoordsToGV(rootNode, listUnit, out);
       out<<"\n}";
       out.flush();
    }
    QString program = "dot";
    //program = "./tools/7z.exe";
    QStringList arguments; arguments.clear(); arguments<< "-Tpdf" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv"
                                            << "-o" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".pdf";
    //Й = new QProcess(this);
    procDot.start(program,arguments);
    procDot.waitForFinished();
}
void DomParser::saveForGraphvizForNode(QString nameFile, Node* rootNode,Node* rootNode2)
{
    QFile file(qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       out.setCodec("UTF-8");
       out<<"digraph pvn {ratio = \"expand\" ; rankdir = \"LR\"  ; \n graph [ ranksep = 20  ]    node [     fontsize = \"16\"           shape = \"ellipse\"      ];  edge [   ];  \n";
       //! находим все модули связанные с данным модулем
       QList<Node* > listUnit;
       //findNeighborUnit(rootNode,listUnit);
       saveNeighborsToGV(rootNode, rootNode2, out);
       out<<"\n}";
       out.flush();
    }
    QString program = "dot";
    //program = "./tools/7z.exe";
    QStringList arguments; arguments.clear(); arguments<< "-Tpdf" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".gv"
                                            << "-o" << qApp->applicationDirPath() +  "/csv/" + nameFile + ".pdf";
    //Й = new QProcess(this);
    procDot.start(program,arguments);
    procDot.waitForFinished();
}


void DomParser::saveAllInfo( Node* rootNode, QTextStream& out)
{
    QVector<QString> list;
    list.resize(E_CORD +1);
    out<<tr("Сигнала;Блок;Идентификатор источника;Разъем;Клемма;Размнож;Направление;Бирка провода;Коммутация(питания/инф);Тип интерфейса;Тип разъема(со стороны блока);Тип разъема(со стороны блока);Тип провода;Тип жилы;Идент. проводов;Схема;Интерфейс;Имя жгута;\n");
    out.flush();

    saveToCVS(rootNode,out,&list);
    out.flush();
}
void DomParser::saveAllUnit( Node* rootNode, QTextStream& out)
{
    QVector<QString> list;
    //list.resize(E_SET_TYPE_I +1);
    out<<tr("Название системы;Идентификатор системы;Идентификатор разъема;\n");
    out.flush();

    recSaveAllUnit(rootNode,out);
    out.flush();
}
void DomParser::recSaveAllUnit(Node* rootNode, QTextStream& out)
{
    if(rootNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode* > (rootNode);
        for(auto i:unit->child)
        {
            ConnectorNode *con = static_cast<ConnectorNode* > (i);
            out<<unit->displayName + ";" +unit->idName + ";" + unit->idName + ":" + con->idName +";\n";
        }
        return;
    }
    for(auto i:rootNode->child)
    {
        recSaveAllUnit(i,out);
    }
}
void DomParser::saveDataToCVS(QString nameFile, Node* rootNode,std::function<void(DomParser&, Node* , QTextStream&)> func, QString strCodec)
{
    QFile file(qApp->applicationDirPath() +  "/csv/" + nameFile + ".csv");
    bool fileOpen = file.open(QIODevice::WriteOnly|QIODevice::Text);
    if(fileOpen == true)
    {
       QTextStream out(&file);
       //out.setCodec("CP1251");
       out.setCodec(strCodec.toLocal8Bit().constData());
       func(*this,rootNode,out);
       out.flush();
    }
}
void DomParser::saveToCVS(Node *startNode, QTextStream& out, QVector<QString> *list)
{

  /*  if(startNode->type() == Node::E_SYSTEM)
        (*list)[E_ID_SYSTEM] = startNode->displayName;
    else*/ if(startNode->type() == Node::E_UNIT)
    {
        (*list)[E_ID_UNIT] = startNode->idName;
        (*list)[E_UNIT_NAME] = startNode->displayName;
    }else if(startNode->type() == Node::E_CONNECTOR)
    {
        ConnectorNode *node = static_cast<ConnectorNode* >(startNode);
        (*list)[E_CONNECTOR]       = startNode->idName;
        (*list)[E_TYPE_CONNECTOR_BLOCK ] = node->typeConnectorBlock;
        (*list)[E_TYPE_CONNECTOR_WIRE ] = node->typeConnectorWire;

        if((*list)[E_TYPE_CONNECTOR_BLOCK ].isEmpty())
            (*list)[E_TYPE_CONNECTOR_BLOCK ] = "-";

        if((*list)[E_TYPE_CONNECTOR_WIRE ].isEmpty())
            (*list)[E_TYPE_CONNECTOR_WIRE ] = "-";

    }else if(startNode->type() == Node::E_PIN)
    {
        (*list)[E_CURCUIT ].clear();
        PinNode *node = static_cast<PinNode* >(startNode);
        (*list)[E_PIN]          = node->idName;
        (*list)[E_NAME ]        = node->nameSignal;
        (*list)[E_IO]           = node->strIO;
        (*list)[E_MULT ]        = node->strNumClone;
        (*list)[E_SW]           = node->strSw;
        (*list)[E_TYPE_I ]      = node->strInterface;
        (*list)[E_TYPE_WIRE ]   = node->strTypeWire;
        (*list)[E_TYPE_WIRE_PIN ]   = node->strTypeWirePin;
        (*list)[E_ID_WIRE ]   = node->strIDWire;


        (*list)[E_SET_TYPE_I ]  = node->strTypeI + node->prefTypeI;
        (*list)[E_CORD ]        = node->strCord;

        for(auto i:node->strCircuit)
        {
            if((*list)[E_CURCUIT ].isEmpty() == false)
                (*list)[E_CURCUIT ]     += ("|" + i);
            else {
                (*list)[E_CURCUIT ]     += (i);
            }
        }

        if((*list)[E_TYPE_I ].isEmpty())
            (*list)[E_TYPE_I ] = "-";

        if((*list)[E_TYPE_WIRE ].isEmpty())
            (*list)[E_TYPE_WIRE ] = "-";

        if((*list)[E_TYPE_WIRE_PIN ].isEmpty())
            (*list)[E_TYPE_WIRE_PIN ] = "-";

        if((*list)[E_ID_WIRE ].isEmpty())
            (*list)[E_ID_WIRE ] = "-";

        if((*list)[E_SET_TYPE_I ].isEmpty())
            (*list)[E_SET_TYPE_I ] = "-";

        if((*list)[E_CORD ].isEmpty())
            (*list)[E_CORD ] = "-";

        if((*list)[E_CURCUIT ].isEmpty())
            (*list)[E_CURCUIT ] = "-";

        if(node->child.isEmpty() == true)
        {
            (*list)[E_LABEL ] = "-";
            //(*list)[E_TYPE_WIRE ] = "-";

            for(auto i:*list)
            {
                out<<i<<";";
            }
            out<<"\n";
        }

    }
    else if(startNode->type() == Node::E_WIRE)
    {
        WireNode *node = static_cast<WireNode* >(startNode);
        (*list)[E_LABEL ] = node->idName;
        for(auto i:*list)
        {
            out<<i<<";";
        }
        out<<"\n";
    }
    QList<Node* > memNode;
    for(auto i:startNode->child)
    {

        if(i->type() == Node::E_WIRE)
        {
            bool skip = false;
            for(auto j:memNode)
            {
                if(j->idName == i->idName)
                    skip = true;
            }
            if(skip==false)
            {
                memNode.push_back(i);
                saveToCVS(i,out,list);
            }
        }else
            saveToCVS(i,out,list);
    }
}
void DomParser::saveNode(Node *startNode, QTextStream& out)
{
    if(startNode->child.isEmpty() == true)
        return;
    for(auto i:startNode->child)
    {
        if(startNode->type() == Node::E_UNIT)
            out<<"\""+startNode->displayName + "\"->\"";
        else
            out<<"\""+startNode->idName + "\"->\"";

        if(i->type() == Node::E_UNIT)
            out<<i->displayName<<"\";\n";
        else
            out<<i->idName<<"\";\n";
        saveNode(i,out);
    }
}
void DomParser::saveNodeInterfaceGW(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_CONNECTOR)
    {
        out<<"subgraph cluster" +  QString::number(startNode->parent->id) + " \n{ fontcolor=\"blue\" fillcolor=\"grey\" style=\"filled\" label=\""+
             startNode->parent->displayName + "["+startNode->parent->idName+"]"+ "\"\n";

        out<<"subgraph cluster" +  QString::number(startNode->id) + " \n{ fontcolor=\"black\" label=\"" + startNode->idName+ "\"\n";

        int k = 0;
        out<<"\"node"+ QString::number(startNode->id) + "\"[ label = \"";

        UnitNode *unit = static_cast<UnitNode* > (startNode->parent);

        for(auto j:unit->interfaces ) //добавляем pin
        {
            if(k > 0)
                out<<"|";
            out<<"<f" + QString::number(k) + "> " + j->strSetI;
            k++;
        }

        out<<"\" \n shape = \"record\"];\n";
        if(startNode->type() == Node::E_CONNECTOR)
        {
            out<<"}\n";
        }
        out<<"}\n";

        for(auto j:unit->interfaces ) //добавляем pin
        {
            auto *pin = j->pins.first();

            if(pin->io == PinNode::E_IN)
            {
                if(pin->child.isEmpty() == true)
                    continue;
                WireNode *wireNode = static_cast<WireNode* > (pin->child.first());

                if(wireNode->toPin == nullptr)
                    continue;
                UnitNode * uNode =static_cast<UnitNode *> (wireNode->toPin->parent->parent);

                InterfaceNode *fInt = nullptr;
                for(auto k:uNode->interfaces)
                {
                    PinNode *pnode = static_cast<PinNode *> (wireNode->toPin);
                    if(k->pins.indexOf(pnode) != -1)
                    {
                        fInt = k;
                        break;
                    }
                }

                    out<<"\""<< fInt->strSetI<<"\"";//идентификатор
                    out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
                     QString::number(unit->interfaces.indexOf(j))+
                     "[label=" + "\"" + j->strSetI + "\"";

//                if(wire->fullConnected == false)
//                    out<<" fillcolor=\"red\" ";
                if(pin->switched == true)
                    out<<" fillcolor=\"blue\" ";

                if(pin->type_interface == PinNode::E_RK)
                    out<<" style=\"dashed\"";
                else if(pin->type_interface == PinNode::E_ARINC_429)
                    out<<" style=\"dotted\"";
                else if(pin->type_interface == PinNode::E_POWER)
                    out<<" style=\"bold\"";
                else
                    out<<" style=\"filled\"";

                out<<"];\n";
                }else if(pin->io == PinNode::E_OUT)
                {
                    out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(unit->interfaces.indexOf(j));

                    out<<"->\""<<j->strSetI + "\"" +"[label=" + "\"" +j->strSetI + "\"";
                    /*if(wire->fullConnected == false)
                      out<<" fillcolor=\"red\" ";
                    else*/ if(pin->switched == true)
                    out<<" fillcolor=\"blue\" ";

                    if(pin->type_interface == PinNode::E_RK)
                        out<<" style=\"dashed\"";
                    else if(pin->type_interface == PinNode::E_ARINC_429)
                        out<<" style=\"dotted\"";
                    else if(pin->type_interface == PinNode::E_POWER)
                        out<<" style=\"bold\"";
                    else
                        out<<" style=\"filled\"";
                    out<<"];\n";

                }

        }
    }

//    if(startNode->type() == Node::E_WIRE)
//    {
//        WireNode *wire = static_cast<WireNode* > (startNode);
//        if(wire->parent->type() == Node::E_PIN)
//        {
//            PinNode *pin = static_cast<PinNode* > (wire->parent);
//            if(pin->io == PinNode::E_IN)
//            {

//               //if(wire->toPin == nullptr)
//                //{
//                    out<<"\""<<wire->idName<<"\"";
//                    out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
//                         QString::number(pin->parent->child.indexOf(pin))+
//                         "[label=" + "\"" + wire->idName + "\"";
//                    if(wire->fullConnected == false)
//                        out<<" fillcolor=\"red\" ";
//                    else if(pin->switched == true)
//                        out<<" fillcolor=\"blue\" ";

//                    if(pin->type_interface == PinNode::E_RK)
//                        out<<" style=\"dashed\"";
//                    else if(pin->type_interface == PinNode::E_ARINC_429)
//                        out<<" style=\"dotted\"";
//                    else if(pin->type_interface == PinNode::E_POWER)
//                        out<<" style=\"bold\"";
//                    else
//                        out<<" style=\"filled\"";

//                    out<<"];\n";

//               // }else
//               //     out<<"\"node" + QString::number(wire->toPin->parent->id) + "\": f"+QString::number(wire->toPin->parent->child.indexOf(wire->toPin));


//            }else if(pin->io == PinNode::E_OUT)
//            {

//                out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(pin->parent->child.indexOf(pin));
//                //if(wire->toPin == nullptr)
//                     out<<"->\""<<wire->idName + "\"" +"[label=" + "\"" +wire->idName + "\"";
//                     if(wire->fullConnected == false)
//                         out<<" fillcolor=\"red\" ";
//                     else if(pin->switched == true)
//                          out<<" fillcolor=\"blue\" ";


//                     if(pin->type_interface == PinNode::E_RK)
//                         out<<" style=\"dashed\"";
//                     else if(pin->type_interface == PinNode::E_ARINC_429)
//                         out<<" style=\"dotted\"";
//                     else if(pin->type_interface == PinNode::E_POWER)
//                         out<<" style=\"bold\"";
//                     else
//                         out<<" style=\"filled\"";


//                         out<<"];\n";
//            }

//        }
//    }
    for(auto i:startNode->child)
    {
        saveNodeInterfaceGW(i,out);
    }

}
void DomParser::saveNodeWiresGW(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_CONNECTOR)
    {
        out<<"subgraph cluster" +  QString::number(startNode->parent->id) + " \n{ fontcolor=\"blue\" fillcolor=\"grey\" style=\"filled\" label=\""+
             startNode->parent->displayName + "["+startNode->parent->idName+"]"+ "\"\n";

        out<<"subgraph cluster" +  QString::number(startNode->id) + " \n{ fontcolor=\"black\" label=\""+startNode->idName+ "\"\n";

        int k =0;
        out<<"\"node"+ QString::number(startNode->id) + "\"[ label = \"";
        for(auto j:startNode->child )
        {
            if(k>0)
                out<<"|";
            out<<"<f"+QString::number(k) + "> " + j->idName;//QString::number(j->id);
            //qDebug()<<qPrintable(j->idName)<<"\n";
            k++;
        }
        out<<"\" \n shape = \"record\"];\n";
        if(startNode->type() == Node::E_CONNECTOR)
        {
            out<<"}\n";
        }
        out<<"}\n";
    }

    if(startNode->type() == Node::E_WIRE)
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->parent->type() == Node::E_PIN)
        {
            PinNode *pin = static_cast<PinNode* > (wire->parent);
            if(pin->io == PinNode::E_IN)
            {

               //if(wire->toPin == nullptr)
                //{
                    out<<"\""<<wire->idName<<"\"";
                    out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
                         QString::number(pin->parent->child.indexOf(pin))+
                         "[label=" + "\"" + wire->idName + "\"";
                    if(wire->fullConnected == false)
                    {
                        //outLog<<"Провод неполностью подключен"<<wire->pathName<<"\n";
                        out<<" fillcolor=\"red\" ";
                    }
                    else if(pin->switched == true)
                        out<<" fillcolor=\"blue\" ";

                    if(pin->type_interface == PinNode::E_RK)
                        out<<" style=\"dashed\"";
                    else if(pin->type_interface == PinNode::E_ARINC_429)
                        out<<" style=\"dotted\"";
                    else if(pin->type_interface == PinNode::E_POWER)
                        out<<" style=\"bold\"";
                    else
                        out<<" style=\"filled\"";

                    out<<"];\n";

               // }else
               //     out<<"\"node" + QString::number(wire->toPin->parent->id) + "\": f"+QString::number(wire->toPin->parent->child.indexOf(wire->toPin));


            }else if(pin->io == PinNode::E_OUT)
            {

                out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(pin->parent->child.indexOf(pin));
                //if(wire->toPin == nullptr)
                     out<<"->\""<<wire->idName + "\"" +"[label=" + "\"" +wire->idName + "\"";
                     if(wire->fullConnected == false)
                     {
                         //outLog<<"Провод неполностью подключен"<<wire->pathName<<"\n";
                         out<<" fillcolor=\"red\" ";
                     }
                     else if(pin->switched == true)
                          out<<" fillcolor=\"blue\" ";


                     if(pin->type_interface == PinNode::E_RK)
                         out<<" style=\"dashed\"";
                     else if(pin->type_interface == PinNode::E_ARINC_429)
                         out<<" style=\"dotted\"";
                     else if(pin->type_interface == PinNode::E_POWER)
                         out<<" style=\"bold\"";
                     else
                         out<<" style=\"filled\"";


                         out<<"];\n";          
            }

        }
    }
    for(auto i:startNode->child)
    {
        saveNodeWiresGW(i,out);
    }

}
void DomParser::saveGraphConnector(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_CONNECTOR)
    {
        out<<"subgraph cluster" +  QString::number(startNode->parent->id) + " \n{ fontcolor=\"blue\" fillcolor=\"grey\" style=\"filled\" label=\""+
             startNode->parent->displayName + "["+startNode->parent->idName+"]"+ "\"\n";

        out<<"subgraph cluster" +  QString::number(startNode->id) + " \n{ fontcolor=\"black\" label=\""+startNode->idName+ "\"\n";

        int k =0;
        out<<"\"node"+ QString::number(startNode->id) + "\"[ label = \"";
        for(auto j:startNode->child )
        {
            if(k>0)
                out<<"|";
            out<<"<f"+QString::number(k) + "> " + j->idName;//QString::number(j->id);

            k++;
        }
        out<<"\" \n shape = \"record\"];\n";
        if(startNode->type() == Node::E_CONNECTOR)
        {
            out<<"}\n";
        }
        out<<"}\n";
    }
}
void DomParser::saveGraphWire(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_WIRE)
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->parent->type() == Node::E_PIN)
        {
            PinNode *pin = static_cast<PinNode* > (wire->parent);
            if(pin->io == PinNode::E_IN)
            {
                    out<<"\""<<wire->idName<<"\"";
                    out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
                         QString::number(pin->parent->child.indexOf(pin))+
                         "[label=" + "\"" + wire->idName + "\"";
                    if(wire->fullConnected == false)
                        out<<" fillcolor=\"red\" ";
                    else if(pin->switched == true)
                        out<<" fillcolor=\"blue\" ";

                    if(pin->type_interface == PinNode::E_RK)
                        out<<" style=\"dashed\"";
                    else if(pin->type_interface == PinNode::E_ARINC_429)
                        out<<" style=\"dotted\"";
                    else if(pin->type_interface == PinNode::E_POWER)
                        out<<" style=\"bold\"";
                    else
                        out<<" style=\"filled\"";

                        out<<"];\n";

            }else
            {

                out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(pin->parent->child.indexOf(pin));
                //if(wire->toPin == nullptr)
                     out<<"->\""<<wire->idName + "\"" +"[label=" + "\"" +wire->idName + "\"";
                     if(wire->fullConnected == false)
                         out<<" fillcolor=\"red\" ";
                     else if(pin->switched == true)
                          out<<" fillcolor=\"blue\" ";
                     if(pin->type_interface == PinNode::E_RK)
                         out<<" style=\"dashed\"";
                     else if(pin->type_interface == PinNode::E_ARINC_429)
                         out<<" style=\"dotted\"";
                     else if(pin->type_interface == PinNode::E_POWER)
                         out<<" style=\"bold\"";
                     else
                         out<<" style=\"filled\"";


                         out<<"];\n";
            }

        }
    }
    for(auto i:startNode->child)
    {
        saveGraphWire(i,out);
    }
}
Node* DomParser::findNodeByType(Node* node, Node::Type t, EDirection dir)
{
    if(node == nullptr)
        return nullptr;
    if(node->type() == t)
        return node;
    if(dir == E_UP)
    {
        Node *r = findNodeByType(node->parent,t,dir);
        if(r != nullptr)
            return r;
    }else
    {
        for(auto i: node->child)
        {
            Node *r = findNodeByType(i,t,dir);
            if(r != nullptr)
                return r;
        }
    }
    return nullptr;
}
void DomParser::recFindConnectedUnit(Node* node, QList<Node* > &listUnit)
{
    bool isFind = false;
    if(node->type() == Node::E_WIRE)
    {
        WireNode *w = static_cast<WireNode* > (node);
        if(w->toPin != nullptr)
        {
            Node* unit = findNodeByType(w->toPin,Node::E_UNIT,EDirection::E_UP);
            UnitNode* unitNode = static_cast<UnitNode* >(unit);
            for(auto i:listUnit)
            {
               if(i->idName == unit->idName)
                  isFind = true;
            }
            // если в списке не было такого элемента, тогда добавить unit в список
            if(isFind == false)
               listUnit.push_back(unit);
           QList<PinNode* > listPin =  unitNode->findAllInternalConnection((PinNode*)w->toPin);
           for(auto n:listPin)
               recFindConnectedUnit(n,listUnit);
        }
    }
    for(auto i:node->child)
    {
        recFindConnectedUnit(i,listUnit);
    }
}
void DomParser::findNeighborUnit(Node* unitNode, QList<Node* > &listUnit)
{
    recFindConnectedUnit(unitNode,listUnit);
}
bool DomParser::checkInListCoords(CoordNode *i,QList<CoordNode*> &listCoords)
{
    QList<WireNode* > wires = i->wires;
    //while(!wires.isEmpty())
    //{
        for(auto w:wires)
        {
      //  WireNode* w = wires.first();
        for(auto i:listCoords)
        {
            if(wires.isEmpty())
                break;
            for(auto w0:i->wires)
            {
                if((w0 == w) || (w->toPin == w0->parent && w->parent == w0->toPin))
                {
                    wires.removeOne(w);
                    w = wires.first();
                    break;
                }
            }
        }
        //if(wires.last() == w)
        //    return false;
    }
    if(wires.isEmpty())
        return true;

    return false;
}
void DomParser::findAllCoordsUnit(Node* unitNode, QList<CoordNode* > &listCoords)
{
    if(unitNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode*> (unitNode);
        for(auto i:unit->coords)
        {
            if(!checkInListCoords(i,listCoords))
                listCoords.append(i);
        }
    }
    for(auto i:unitNode->child)
    {
        findAllCoordsUnit(i,listCoords);
    }
}
void DomParser::saveNodeVarWithNe(Node *startNode, QTextStream& out)
{
    if(startNode->type() == Node::E_CONNECTOR)
    {
        out<<"subgraph cluster" +  QString::number(startNode->parent->id) + " \n{ fontcolor=\"blue\" fillcolor=\"grey\" style=\"filled\" label=\""+
             startNode->parent->displayName + "["+startNode->parent->idName+"]"+ "\"\n";

        out<<"subgraph cluster" +  QString::number(startNode->id) + " \n{ fontcolor=\"black\" label=\""+startNode->idName+ "\"\n";

        int k =0;
        out<<"\"node"+ QString::number(startNode->id) + "\"[ label = \"";
        for(auto j:startNode->child )
        {
            if(k>0)
                out<<"|";
            out<<"<f"+QString::number(k) + "> " + j->idName;//QString::number(j->id);

            k++;
        }
        out<<"\" \n shape = \"record\"];\n";
        if(startNode->type() == Node::E_CONNECTOR)
        {
            out<<"}\n";
        }
        out<<"}\n";
    }

    if(startNode->type() == Node::E_WIRE)
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->parent->type() == Node::E_PIN)
        {
            PinNode *pin = static_cast<PinNode* > (wire->parent);
            if(pin->io == PinNode::E_IN)
            {


                    out<<"\""<<wire->idName<<"\"";
                    out<<"->"<<"\"node" + QString::number(pin->parent->id) + "\": f"+
                         QString::number(pin->parent->child.indexOf(pin))+
                         "[label=" + "\"" + wire->idName + "\"";
                    if(wire->fullConnected == false)
                        out<<" fillcolor=\"red\" ";
                    else if(pin->switched == true)
                        out<<" fillcolor=\"blue\" ";

                    if(pin->type_interface == PinNode::E_RK)
                        out<<" style=\"dashed\"";
                    else if(pin->type_interface == PinNode::E_ARINC_429)
                        out<<" style=\"dotted\"";
                    else if(pin->type_interface == PinNode::E_POWER)
                        out<<" style=\"bold\"";
                    else
                        out<<" style=\"filled\"";

                        out<<"];\n";
                        if(wire->toPin != nullptr)
                        {
                            saveGraphConnector(wire->toPin->parent,out);
                            saveGraphWire(wire->toPin,out);
                        }
            }else
            {

                out<<"\"node" + QString::number(pin->parent->id) + "\": f"+QString::number(pin->parent->child.indexOf(pin));
                //if(wire->toPin == nullptr)
                     out<<"->\""<<wire->idName + "\"" +"[label=" + "\"" +wire->idName + "\"";
                     if(wire->fullConnected == false)
                     {

                         out<<" fillcolor=\"red\" ";
                     }
                     else if(pin->switched == true)
                          out<<" fillcolor=\"blue\" ";


                     if(pin->type_interface == PinNode::E_RK)
                         out<<" style=\"dashed\"";
                     else if(pin->type_interface == PinNode::E_ARINC_429)
                         out<<" style=\"dotted\"";
                     else if(pin->type_interface == PinNode::E_POWER)
                         out<<" style=\"bold\"";
                     else
                         out<<" style=\"filled\"";

                   out<<"];\n";

                  if(wire->toPin != nullptr)
                  {
                      saveGraphConnector(wire->toPin->parent,out);
                      saveGraphWire(wire->toPin,out);
                  }
            }

        }
    }
    for(auto i:startNode->child)
    {
        saveNodeVarWithNe(i,out);
    }

}
void DomParser::correctWire(Node *startNode)
{
    if(startNode->type() == Node::E_WIRE)
    {
        WireNode *wire = static_cast<WireNode* > (startNode);
        if(wire->toPin !=nullptr)
        {
              PinNode *pin = static_cast<PinNode* > (wire->toPin);
             /* if(pin == nullptr)
              {
                  UnitNode* parent0 = static_cast<UnitNode *> (findNodeByType(pin,Node::E_UNIT,EDirection::E_UP));
                  if(parent0->alias.isEmpty() == false)
                  {
                      wire->idName = parent0->alias +
                           pin->parent->idName + pin->idName;
                  }
              }
              else */
              if(pin != nullptr)
              {
                PinNode *curPin = static_cast<PinNode* > (wire->parent);
                if(curPin->io == PinNode::E_OUT)
                {
                    UnitNode* parent0 = static_cast<UnitNode *> (findNodeByType(curPin,Node::E_UNIT,EDirection::E_UP));

                    if(parent0->alias.isEmpty() == true)
                    {
                        wire->idName = parent0->idName + "-" +
                             curPin->parent->idName +"-" + curPin->idName;
                    }else
                    {
                        wire->idName = parent0->alias +
                             curPin->parent->idName + curPin->idName;
                    }
                    for(auto i : vecAlias)
                    {
                        if(wire->idName.contains(i.first,Qt::CaseInsensitive))
                        {
                            wire->idName.replace(i.first,i.second);
                        }
                    }


                }else if(wire->fullConnected)
                {

                    UnitNode* parent0 = static_cast<UnitNode *> (findNodeByType(pin,Node::E_UNIT,EDirection::E_UP));

                    if(parent0->alias.isEmpty() == true)
                    {
                        wire->idName = parent0->idName + "-" +
                             pin->parent->idName +"-" + pin->idName;
                    }else
                    {
                        wire->idName = parent0->alias +
                             pin->parent->idName + pin->idName;
                    }

                    for(auto i : vecAlias)
                    {
                        if(wire->idName.contains(i.first,Qt::CaseInsensitive))
                        {
                            wire->idName.replace(i.first,i.second);
                        }
                    }

//                    wire->idName = pin->parent->parent->idName + "-" +
//                               pin->parent->idName +"-" + pin->idName;

                    //curPin->strTypeI = pin->strTypeI;

                }
              }
        }
    }
    for(auto i:startNode->child)
    {
        correctWire(i);
    }
}
void DomParser::checkConnectionInterfaces(Node *startNode)
{
    if(startNode->type() == Node::E_UNIT) //Если блок
    {
        UnitNode *unit = static_cast<UnitNode* > (startNode);
        for(auto i:unit->interfaces)
        {
            if(i->pins.first()->io == PinNode::E_OUT)
            {
                for(auto j:i->pins)
                {
                    for( auto k:j->child)
                    {
                        if(k->type() == Node::E_WIRE)
                        {
                            WireNode *wire = static_cast<WireNode* > (k);
                            if(wire->fullConnected)
                            {
                                PinNode *toPin = static_cast<PinNode* > (wire->toPin);
                                toPin->strTypeI = j->strTypeI;
                                toPin->prefTypeI= j->prefTypeI;
                            }
                        }
                    }
                }
            }
        }
       return;
    }

    for(auto i:startNode->child)
    {
        //! уходим в глубь в поисках узла "Блок"
        checkConnectionInterfaces(i);
    }


}

void DomParser::calcNumInterface(Node *startNode)
{

}
void DomParser::renameCoords(Node *startNode)
{
    if(startNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode* > (startNode);
        unit->renameCoords(unit);
        return;
    }
    for(auto i:startNode->child)
    {
        renameCoords(i);
    }
}
void DomParser::clearCoords(Node *startNode)
{
    if(startNode->type() == Node::E_UNIT)
    {

        UnitNode *unit = static_cast<UnitNode* > (startNode);
//        if(unit->idParentSys == "ИМК")
//            return;
        unit->coords.clear();
       // unit->calcInterface();
        return;
    }
    for(auto i:startNode->child)
    {
        clearCoords(i);
    }
}
void DomParser::correctCoords(Node* startNode)
{
    if(startNode->type() == Node::E_UNIT)
    {

        UnitNode *unit = static_cast<UnitNode* > (startNode);
        if(unit->idParentSys == "ИМК")
            return;
        unit->scanCoords(unit);
       // unit->calcInterface();
        return;
    }
    for(auto i:startNode->child)
    {
        correctCoords(i);
    }

}
void DomParser::correctInterface(Node *startNode)
{
    if(startNode->type() == Node::E_UNIT)
    {
        UnitNode *unit = static_cast<UnitNode* > (startNode);
        unit->scanInterface(unit);
        unit->calcInterface();
        return;
    }
    for(auto i:startNode->child)
    {
        correctInterface(i);
    }
}
DomParser::~DomParser() {
        // TODO Auto-generated destructor stub
    procDot.terminate();
}
