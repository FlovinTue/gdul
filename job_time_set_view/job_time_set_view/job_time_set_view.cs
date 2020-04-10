using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Xml;

namespace job_time_set_view
{
    using job_info = List<string>;

    public partial class job_time_set_view : Form
    {
        private Dictionary<string, job_info> m_jobInfos;

        // dataSource -> dataVariation -> value list with links to corresponding job_info
        private Dictionary<string, Dictionary<string, List<Tuple<float, job_info>>>> m_dataSources;

        // for string parse conversion
        private System.Globalization.CultureInfo m_enCultureInfo;

        public job_time_set_view()
        {
            m_jobInfos = new Dictionary<string, job_info>();
            m_dataSources = new Dictionary<string, Dictionary<string, List<Tuple<float, job_info>>>>();

            m_enCultureInfo = new System.Globalization.CultureInfo("en-US");

            InitializeComponent();
        }

        private void job_time_set_view_on_drag_drop(object sender, System.Windows.Forms.DragEventArgs e)
        {
            var dropped = e.Data.GetData(DataFormats.FileDrop);
            if (dropped.GetType() != typeof(System.String[]))
            {
                e.Effect = DragDropEffects.None;
                return;
            }
            string[] filesDropped = dropped as string[];
            string first = filesDropped[0];

            if (!first.Contains("job_time_sets.xml"))
            {
                e.Effect = DragDropEffects.None;
                return;
            }

            XmlDocument newDoc = new XmlDocument();
            newDoc.Load(first);

            parse_xml(newDoc);
        }
        private void job_time_set_view_drag_enter(object sender, System.Windows.Forms.DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.All;
            else
                e.Effect = DragDropEffects.None;
        }
        private void parse_xml(XmlDocument document)
        {
            if (!document.HasChildNodes)
                return;

            XmlNodeList jobs = document.GetElementsByTagName("jobs");

            if (jobs == null)
                return;

            for(var i = 0; i < jobs[0].ChildNodes.Count; ++i)
            {
                XmlNode job = jobs[0].ChildNodes[i];
                XmlAttributeCollection jobAttrib = job.Attributes;
                XmlNode jobIdentifier = jobAttrib.Item(0);
                string jobIdStr = jobIdentifier.Value.ToString();
                job_info info = new job_info();

                m_jobInfos.Add(jobIdStr, info);

                info.Add(jobIdentifier.Name + ": " + jobIdentifier.Value.ToString());

                for(var mem = 0; mem < job.ChildNodes.Count; ++mem)
                {
                    XmlNode member = job.ChildNodes[mem];

                    if (member.Name == "time_set")
                    {
                        string setName = member.Attributes.Item(0).Value.ToString();
                        if (!m_dataSources.ContainsKey(setName))
                            m_dataSources.Add(setName, new Dictionary<string, List<Tuple<float, job_info>>>());

                        var dataSource = m_dataSources[setName];
                        for (var tsIndex = 0; tsIndex < member.ChildNodes.Count; ++tsIndex)
                        {
                            XmlNode tsMember = member.ChildNodes[tsIndex];

                            string tsMemberName = tsMember.Name;

                            if (!dataSource.ContainsKey(tsMemberName))
                                dataSource.Add(tsMemberName, new List<Tuple<float, job_info>>());

                            string valueStr = tsMember.FirstChild.Value;
                            float value = float.Parse(valueStr, m_enCultureInfo);

                            dataSource[tsMemberName].Add(new Tuple<float, job_info>(value, info));
                            info.Add(setName + "_" + tsMemberName + ": " + valueStr);
                        }
                        continue;
                    }

                    info.Add(member.Name + ": " + member.FirstChild.Value.ToString());
                }
            }
        }
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void job_time_set_view_Load(object sender, EventArgs e)
        {

        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void tabPage1_Click(object sender, EventArgs e)
        {

        }
        private void tabPage2_Click(object sender, EventArgs e)
        {

        }
        private void tabPage3_Click(object sender, EventArgs e)
        {

        }

        private void listBox1_SelectedIndexChanged_1(object sender, EventArgs e)
        {

        }

        private void label2_Click(object sender, EventArgs e)
        {

        }

        private void chart1_Click(object sender, EventArgs e)
        {

        }
    }
}
